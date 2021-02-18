/* 
 * Algoritmi paraleli si distribuiti
 * Tema 3 -Procesare imagini
 * 
 * Author: Stefan-Dobrin Cosmin
 * 331CA
 *
 */
#include <errno.h>
#include <values.h>
#include "img_process.h"
#include "filters.h"

int WIDTH;
int HEIGHT;
int MAX_COLOR;

U8 *fullImage;
U8 **imageStrip;
short **imageResidual;
short *fullImageResidual;

int rank,numberProcesses;
int stripSize,stripStart,stripEnd;


/* Citeste datele de intrare.*/
void readData(char* inputFile)
{
    FILE* fin;
    char data[70];

    if((fin=fopen(inputFile,"r"))==NULL)
    {
        perror("Error opening input file");
        exit(1);
    }

    printf("Reading input file HEADER:\n");

    //Reading format
    fscanf(fin,"%s",data);
    while(data[0]=='#') //ignoring comments
    {
        printf("\tFound comment: %s",data);
        fgets(data,70,fin);
        printf("%s",data);
        fscanf(fin,"%s",data);
    }
    if(strcasecmp(data,"P2")!=0)
    {
        printf("Illegal format type of input file. Expected encoding: \"P2\". Found encoding \"%s\"\n",data);
        exit(1);
    }
    else
        printf("\tFile format found for %s: %s\n",inputFile,data);

    //Reading Width
    fscanf(fin,"%s",data);
    while(data[0]=='#') //ignoring comments
    {
        printf("\tFound comment: %s",data);
        fgets(data,70,fin);
        printf("%s",data);
        fscanf(fin,"%s",data);
    }
    sscanf(data,"%d",&WIDTH);
    printf("\tImage width: %d\n",WIDTH);

    //Reading Height
    fscanf(fin,"%s",data);
    while(data[0]=='#') //ignoring comments
    {
        printf("\tFound comment: %s",data);
        fgets(data,70,fin);
        printf("%s",data);
        fscanf(fin,"%s",data);
    }
    sscanf(data,"%d",&HEIGHT);
    printf("\tImage height: %d\n",HEIGHT);


    //Reading MAX_COLOR
    fscanf(fin,"%s",data);
    while(data[0]=='#') //ignoring comments
    {
        printf("\tFound comment: %s",data);
        fgets(data,70,fin);
        printf("%s",data);
        fscanf(fin,"%s",data);
    }
    sscanf(data,"%d",&MAX_COLOR);
    printf("\tImage number of colors: %d\n",MAX_COLOR);

    //Reading data

    printf("Reading input file DATA.\n");
    fullImage=(U8*)malloc(WIDTH*HEIGHT*sizeof(U8*));
    int i,j,temp;

    for(i=0;i<HEIGHT;i++)
        for(j=0;j<WIDTH;j++)
        {
            fscanf(fin,"%d",&temp);
            fullImage[i*WIDTH+j]=temp;
        }
    fclose(fin);
}

/* Scrie fisierul de iesire, folosind datele din fullImage */
void writeData(char* outputFile)
{
    FILE* fout;
    int i,j;

    printf("[Proces %d] Writing output file.\n",rank);

    if((fout=fopen(outputFile,"w"))==NULL)
    {
        perror("Error opening output file ");
        exit(1);
    }

    //Scriere header
    fprintf(fout,"%s\n%d %d\n%d\n","P2",WIDTH,HEIGHT,MAX_COLOR);
    fprintf(fout,"#Created by CosminSD\n");
    //Scriere date
    for(i=0;i<HEIGHT;i++)
        for(j=0;j<WIDTH;j++)
            fprintf(fout,"%d\n",fullImage[i*WIDTH+j]);
    fclose(fout);

    printf("[Proces %d]\tWriting finished output file.\n",rank);
}

/* Write the output image of the residual image, using the data in fullImageResidual */
void writeDataResidual(char* outputFile)
{
    FILE* fout;
    int i,j;

    printf("[Proces %d] Writing output file.\n",rank);

    if((fout=fopen(outputFile,"w"))==NULL)
    {
        perror("Error opening output file");
        exit(1);
    }

    //Write header
    fprintf(fout,"%d %d\n",WIDTH,HEIGHT);
    //Scriere date
    for(i=0;i<HEIGHT;i++)
        for(j=0;j<WIDTH;j++)
            fprintf(fout,"%d\n",fullImageResidual[i*WIDTH+j]);
    fclose(fout);

    printf("[Proces %d]\tWriting output file completed.\n",rank);
}


//The function that adjusts the contrast of the image:
//If = (b-a) * (Ii -min) / (max-min) + a
void contrast(int a,int b)
{
    int i,j;
    int min=MAX_COLOR+1;
    int max=0;
    int minAbsolut,maxAbsolut;

    printf("[Proces 0] We start adjusting the image contrast.\n");
    //Min / max calculation per band
    for(i=1;i<=stripSize;i++)
        for(j=1;j<=WIDTH;j++)
        {
            if(imageStrip[i][j]>max)
                max=imageStrip[i][j];
            if(imageStrip[i][j]<min)
                min=imageStrip[i][j];
        }
    printf("[Proces %d]\tMin band: %d, Max band: %d\n",rank,min,max);
    MPI_Allreduce(&min,&minAbsolut,1,MPI_INT,MPI_MIN,MPI_COMM_WORLD);
    MPI_Allreduce(&max,&maxAbsolut,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);

    if(rank==0)
         printf("[Proces %d]\tMin absolut: %d, Max absolut: %d\n",rank,minAbsolut,maxAbsolut);

    //Ajustarea contrastului
    for(i=1;i<=stripSize;i++)
        for(j=1;j<=WIDTH;j++)
            imageStrip[i][j]= (b-a) * (imageStrip[i][j] -minAbsolut) / (maxAbsolut-minAbsolut) + a;

    printf("[Proces %d]\tContrast adjustment completed.\n",rank);
}

/* Apply an image filter . */
void filters(char* filterName)
{
    int filterType;
    int *filterMatrix;
    int i,j;

    //Setting the filter type
    if(strcasecmp(filterName,"identity")==0)
    {
        filterType=F_IDENTITY_C;
        filterMatrix=F_IDENTITY;
    } else
    if(strcasecmp(filterName,"smooth")==0)
    {
        filterType=F_SMOOTH_C;
        filterMatrix=F_SMOOTH;
    } else
    if(strcasecmp(filterName,"blur")==0)
    {
        filterType=F_BLUR_C;
        filterMatrix=F_BLUR;
    } else
    if(strcasecmp(filterName,"sharpen")==0)
    {
        filterType=F_SHARPEN_C;
        filterMatrix=F_SHARPEN;
    } else
    if(strcasecmp(filterName,"mean_remove")==0)
    {
        filterType=F_MEAN_REMOVE_C;
        filterMatrix=F_MEAN_REMOVE;
    } else
    if(strcasecmp(filterName,"emboss")==0)
    {
        filterType=F_EMBOSS_C;
        filterMatrix=F_EMBOSS;
    } else
    {
	fprintf(stderr,"[ERROR] Mod incorect: %s. The execution terminated.\n",filterName);
	MPI_Abort(MPI_COMM_WORLD,1);
	exit(1);
    }

    printf("[Proces %d] Set filter mode  %s. Communication begins to obtain the necessary information.\n",rank,filterName);

    //Obtaining the extra-required lines from the neighbouring processes - MPI Communication
    MPI_Status status;
    //We receive from above - if not the first
    if(rank!=0)
	MPI_Recv(imageStrip[0]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank-1,1,MPI_COMM_WORLD,&status);

    //We send it down - if it's not the last
    if(rank!=numberProcesses-1)
	MPI_Send(imageStrip[stripSize]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank+1,1,MPI_COMM_WORLD);

    //we receive from below - if it is not the last
    if(rank!=numberProcesses-1)
	MPI_Recv(imageStrip[stripSize+1]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank+1,1,MPI_COMM_WORLD,&status);

    //We send up - if not the first
    if(rank!=0)
	MPI_Send(imageStrip[1]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank-1,1,MPI_COMM_WORLD);

    printf("[Proces %d] \tAuxiliary data received.\n",rank); // The extension strip is:\n\t",rank);

    //We allocate space for the output matrix
    //Space allocation with memory check
    /* The imageStripBackup matrix will be used, in which we will store the resulting image */
    U8** imageStripBackup=(U8**)malloc((stripSize+2)*sizeof(U8*));
    if (imageStripBackup == NULL) {
        fprintf(stderr,"[ERROR] Process %d could not allocate any more memory.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for(i=0;i<=stripSize+1;i++)
    {
        imageStripBackup[i]=(U8*)calloc((WIDTH + 2),sizeof(U8));
        if (imageStripBackup[i] == NULL) {
            fprintf(stderr,"[ERROR] Process %d could not allocate any more memory for strip row %d.\n", rank,i);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }

    //Aplicam filtrul
    int sum,k;
    for(i=1;i<=stripSize;i++)
	for(j=1;j<=WIDTH;j++)
	{
	    sum=0;
	    //Linia anterioara
	    for(k=-1;k<=1;k++)
		sum+=filterMatrix[k+1]*imageStrip[i-1][j+k];
	    
	    //Linia curenta
	    for(k=-1;k<=1;k++)
		sum+=filterMatrix[k+1+3]*imageStrip[i][j+k];

	    //Linia urmatoare
	    for(k=-1;k<=1;k++)
		sum+=filterMatrix[k+1+6]*imageStrip[i+1][j+k];

	    sum=sum/F_FACTORS[filterType]+F_OFFSETS[filterType];
	    if(sum>MAX_COLOR)
		sum=MAX_COLOR;
	    if(sum<0)
		sum=0;
	    imageStripBackup[i][j]=sum;
	}

    //Free imageStrip
    for(i=0;i<stripSize+2;i++)
	free(imageStrip[i]);
    free(imageStrip);

    //We set the imageStripBackup matrix as imageStrip, to be written to the file
    imageStrip=imageStripBackup;
    printf("[Proces %d] \tThe filter was applied successfully .\n",rank);


}


void trimiteImageResidual()
{
    MPI_Request *requests;
    MPI_Status *statuses;

    if(rank==MASTER)
    {
	fullImageResidual=(short*)malloc(WIDTH*HEIGHT*sizeof(U8*));
	requests=(MPI_Request*)malloc(sizeof(MPI_Request)*numberProcesses);
        statuses=(MPI_Status*)malloc(sizeof(MPI_Status)*numberProcesses);
    }
    
    int i,j,pozCerere;
    //Colectare informatii
    if(rank != MASTER )
    {
        printf("[Proces %d] \tWe send the strip from the residual image to the master process.\n",rank);
        //We copy the data in a buffer, to be transmitted simultaneously
        short* buffer=(short*) malloc (stripSize*WIDTH*sizeof(short));
        for(i=0;i<stripSize;i++)
            for(j=0;j<WIDTH;j++)
                buffer[i*WIDTH + j]=imageResidual[i][j];
        //Send the data

        MPI_Send(buffer,stripSize*WIDTH,MPI_SHORT,MASTER,rank,MPI_COMM_WORLD);
        //Free memory
        free(buffer);
    }
    //MASTER PROCESS
    else
    {
        printf("[MASTER] \tWe receive the strips dim the residual image from the rest of the processes.\n");
        int sendStripeSize=HEIGHT/ numberProcesses;
        int initialStripSize=sendStripeSize;
        pozCerere=0;
        //copy the data from each process to the appropriate location in fullImage 
        for(i=0;i<numberProcesses;i++)
            if(i!=MASTER)
            {
                printf("[MASTER] \tReceive the data from the process %d.\n",i);
                if(i==numberProcesses-1)
                    sendStripeSize = HEIGHT - sendStripeSize * (numberProcesses-1);
                MPI_Irecv (fullImageResidual + i*initialStripSize*WIDTH,sendStripeSize*WIDTH,MPI_SHORT,i,i,MPI_COMM_WORLD,&requests[pozCerere++]);
            }
        //Put the data of the MASTER process in the imageStrip
        for(i=0;i<stripSize;i++)
            for(j=0;j<WIDTH;j++)
            {
                fullImageResidual[rank*WIDTH + i*WIDTH + j]=imageResidual[i][j];
            }

        printf("[MASTER]\twaiting to receive the data.\n");
        MPI_Waitall(numberProcesses-1,requests,statuses);
    }
    
}


/*Perform task 3 - entropy calculation  */
double entropy(float a, float b, float c)
{
    //Obtaining the extra-required lines from the neighbouring processes - MPI Communication
    MPI_Status status;
    //We receive from above - if not the first
    if(rank!=0)
	MPI_Recv(imageStrip[0]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank-1,1,MPI_COMM_WORLD,&status);

    //We send it down - if it's not the last
    if(rank!=numberProcesses-1)
	MPI_Send(imageStrip[stripSize]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank+1,1,MPI_COMM_WORLD);

    //we receive from below - if it is not the last
    if(rank!=numberProcesses-1)
	MPI_Recv(imageStrip[stripSize+1]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank+1,1,MPI_COMM_WORLD,&status);

    //We send up - if not the first
    if(rank!=0)
	MPI_Send(imageStrip[1]+sizeof(U8),WIDTH,MPI_UNSIGNED_CHAR,rank-1,1,MPI_COMM_WORLD);

    printf("[Proces %d] \tAuxiliary data received .\n",rank);

    //Residual image calculation

    int i,j;

    //Allocation of space for the residual matrix, with allocation verification

    /* The array has stripSize rows and WIDTH columns => 0 small offset from imageStrip. */
    imageResidual=(short**)malloc(stripSize*sizeof(short*));
    if (imageResidual == NULL) {
        fprintf(stderr,"[ERROR] Process %d could not allocate any more memory for residual matrix.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for(i=0;i<stripSize;i++)
    {
        imageResidual[i]=(short*)calloc((WIDTH),sizeof(short));
        if (imageResidual[i] == NULL) {
            fprintf(stderr,"[ERROR] Process %d could not allocate any more memory for residual matrix row %d.\n", rank,i);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }

    /***** Calculate PREDICTOR + RESIDUAL IMAGE  ****/
    /*Simultaneous calculation of the minimum and maximum per band. The minimum and maximum will be used for counting **/
    short min=MAXSHORT;
    short max=-MAXSHORT;
    for(i=1;i<=stripSize;i++)
	for(j=1;j<=WIDTH;j++)
	{
	    //predictor
	    imageResidual[i-1][j-1]=ceil(a*imageStrip[i-1][j] + b*imageStrip[i-1][j-1] + c*imageStrip[i][j-1]);
	    //residual image
	    imageResidual[i-1][j-1]=imageStrip[i][j]-imageResidual[i-1][j-1];

	    //maximum and minimum
	    if(imageResidual[i-1][j-1]>max)
		max=imageResidual[i-1][j-1];
	    if(imageResidual[i-1][j-1]<min)
		min=imageResidual[i-1][j-1];
	}

    printf("[Proces %d] \tS-a finished calculating the residual image.\n",rank);
/*
    for(i=0;i<stripSize;i++,printf("\n"))
	for(j=0;j<WIDTH;j++)
	    printf("%4d ",imageResidual[i][j]);
*/

    //residual image communication

    trimiteImageResidual();

    //Global minimum and maximum communication

    short minTemp,maxTemp;
    MPI_Reduce(&min,&minTemp,1,MPI_SHORT,MPI_MIN,MASTER,MPI_COMM_WORLD);
    MPI_Reduce(&max,&maxTemp,1,MPI_SHORT,MPI_MAX,MASTER,MPI_COMM_WORLD);
    min=minTemp; max=maxTemp;
    
    //Only the master process calculates the entropy

    if(rank==MASTER)
    {
	printf("[Proces %d]\tMin absolut: %d, Max absolut: %d\n",rank,minTemp,maxTemp);

	//Allocating space and counting the number of appearances for each symbol in the residualImage
	unsigned* counter=(unsigned*)calloc(max-min+1,sizeof(unsigned));
	for(i=0;i<HEIGHT*WIDTH;i++)
	    counter[fullImageResidual[i]-min]++;

	//Calculating the entropy
	double entropy=0,probability=0;
	for(i=0;i<max-min+1;i++)
	{
	    if(counter[i]==0)
		continue;
	    probability=(double)counter[i]/WIDTH/HEIGHT;
	    entropy=entropy - probability*log2(probability);
	    //printf("%d(%d -> %f) ",i+min,counter[i],probability,entropy);
	}

	return entropy;
    }


    return 0;
}

/*
 * 
 */
int main(int argc, char** argv)
{
    char* outputFileName;
    //Initial parameters check
    if(argc<3)
    {
        fprintf(stderr,"\nIncorrect number of parameters. Use :\n%s contrast/filter/entropy input_file ...\n",argv[0]);
        return EXIT_FAILURE;
    }

    /***************** Verify the parameters *****************************/
    if(strcasecmp(argv[1],"contrast")==0)
    {
        if(argc!=6)
        {
            fprintf (stderr,"\nIncorrect number of parameters. Use :\n%s contrast input_file a b output_file\n",argv[0]);
            return EXIT_FAILURE;
        }
        outputFileName=argv[5];
    }

    if(strcasecmp(argv[1],"filter")==0)
    {
        if(argc!=5)
        {
            fprintf (stderr,"\nIncorrect number of parameters. Use :\n%s filter input_file smooth/blur/sharpen/mean_remove/emboss output_file\n",argv[0]);
            return EXIT_FAILURE;
        }
        outputFileName=argv[4];
    }

    if(strcasecmp(argv[1],"entropy")==0)
    {
        if(argc!=7)
        {
            fprintf (stderr,"\nIncorrect number of parameters. Use :\n%s entropy input_file a b c output_file\n",argv[0]);
            return EXIT_FAILURE;
        }
        outputFileName=argv[6];
    }


    /*********************** MPI STARTING FROM HERE ************************/

    MPI_Status status;
    MPI_Request *requests;
    MPI_Status *statuses;
    int rc;
    int i,j;
    int bufferInt[20];

    
    // initializare MPI
    rc = MPI_Init(&argc,&argv);
    if (rc != MPI_SUCCESS)
    {
        fprintf (stderr,"Error starting MPI program. Terminating...\n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numberProcesses);

    //Citire imagine si initializari
    if(rank==MASTER)
    {
        readData(argv[2]);
        requests=(MPI_Request*)malloc(sizeof(MPI_Request)*numberProcesses);
        statuses=(MPI_Status*)malloc(sizeof(MPI_Status)*numberProcesses);
    }

/*
    if(rank==MASTER)
    {
    printf("[Proces %d] S-au citit datele:\n",rank);
    for(i=0;i<HEIGHT;i++,printf("\n"))
        for(j=0;j<WIDTH;j++)
            printf("%3d ",fullImage[i*WIDTH+j]);
    }
*/

    /************************ INITIAL COMMUNICATION  ****************************/
    //image size transmission to processes

    bufferInt[0]=WIDTH;
    bufferInt[1]=HEIGHT;
    bufferInt[2]=MAX_COLOR;
    MPI_Bcast (&bufferInt,3,MPI_INT,MASTER,MPI_COMM_WORLD);

    if(rank!=MASTER)
    {
        WIDTH=bufferInt[0];
        HEIGHT=bufferInt[1];
        MAX_COLOR=bufferInt[2];        
    }

    //Strip size calculation

    stripSize =  HEIGHT/ numberProcesses;
    stripStart = rank*stripSize;
    stripEnd = rank*stripSize+stripSize-1;
    if (rank == numberProcesses-1 && HEIGHT % numberProcesses != 0)
    {
	stripSize = HEIGHT - stripSize * (numberProcesses-1);
        stripEnd=HEIGHT-1;
    }

    printf("\n[Proces %d] Image dimension: %d X %d => size band: %d (%d..%d)\n",rank,WIDTH,HEIGHT,stripSize,stripStart,stripEnd);


    //Space allocation with memory check

    /* The imageStrip matrix will be used, in which row 0 represents the row in the image before this phase,
     * and the stripSize + 1 line will represent the row in the image after this strip. These rows will be used
     * in tasks 2 and 3. Also, the rows have dimension WIDTH + 2, the data being between indices 1 and WIDTH,
     * to make the procedure easier in task 2 (the edges have the value 0).*/
    imageStrip=(U8**)malloc((stripSize+2)*sizeof(U8*));
    if (imageStrip == NULL) {
        fprintf(stderr,"[ERROR] Process %d could not allocate any more memory.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for(i=0;i<=stripSize+1;i++)
    {
        imageStrip[i]=(U8*)calloc((WIDTH + 2),sizeof(U8));
        if (imageStrip[i] == NULL) {
            fprintf(stderr,"[ERROR] Process %d could not allocate any more memory for strip row %d.\n", rank,i);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    
    //data transfer. The MASTER process sends the information. All lines in the matrix are sent, linearized

    int pozCerere=0;
    if(rank==MASTER)
    {
        int sendStripeSize=HEIGHT/ numberProcesses;
        int initialStripSize=sendStripeSize;
        for(i=0;i<numberProcesses;i++)
            if(i!=MASTER)
            {
                if(i==numberProcesses-1)
                    sendStripeSize = HEIGHT - sendStripeSize * (numberProcesses-1);
                printf("[MASTER] send the size strip  %d, incepand la pozitia %d, la procesul %d.\n",sendStripeSize, i*initialStripSize*WIDTH,i);
                MPI_Isend (fullImage + i*initialStripSize*WIDTH,sendStripeSize*WIDTH,MPI_UNSIGNED_CHAR,i,i,MPI_COMM_WORLD,&requests[pozCerere++]);
                //MPI_Send (fullImage + i*initialStripSize*WIDTH,sendStripeSize*WIDTH,MPI_UNSIGNED_CHAR,i,i,MPI_COMM_WORLD);
            }
        //Punem si datele procesului MASTER in imageStrip
        U8* bufferChar=fullImage+MASTER*initialStripSize*WIDTH;
        for(i=0;i<stripSize;i++)
            for(j=0;j<WIDTH;j++)
            {
                //printf("MASTER: Put in %d,%d ce e la %d: %d\n",i+1,j+1,i*WIDTH+j,bufferChar[i*WIDTH+j]);
                imageStrip[i+1][j+1]=bufferChar[i*WIDTH+j];
            }

        printf("[MASTER] Waiting for transimission.\n");
        MPI_Waitall(numberProcesses-1,requests,statuses);
    }
    else
    {
        //Lineage strip lines are received

        U8* bufferChar=(U8*)malloc(sizeof(U8)*WIDTH*stripSize);
        printf("[Proces %d] We start receiving data\n",rank);
        MPI_Recv(bufferChar,stripSize*WIDTH,MPI_UNSIGNED_CHAR,MASTER,rank,MPI_COMM_WORLD,&status);
        printf("[Proces %d] \tData received, we begin delineation.\n",rank);

        //The data is delineated. The last and first row / column are excluded, which remain 0

        for(i=0;i<stripSize;i++)
            for(j=0;j<WIDTH;j++)
            {
                //printf("[Proces %d] Received (%d,%d): %d\n",rank,(i+1),j+1,bufferChar[i*WIDTH+j]);
                imageStrip[i+1][j+1]=bufferChar[i*WIDTH+j];
            }
    }

/*
    printf("[Proces %d] \tS-a primit fasia:\n",rank);
    for(i=1;i<=stripSize;i++,printf("\n"))
        for(j=1;j<=WIDTH;j++)
            printf("%3d ",imageStrip[i][j]);
*/

    /********************* END OF INITIAL COMMUNICATION  ***************************/
   
    /**************************** PROCESSING ***********************************/

    //Adjust contrast
    if(strcasecmp(argv[1],"contrast")==0)
    {
        int a,b;
        sscanf(argv[3],"%d",&a);
        sscanf(argv[4],"%d",&b);
        contrast(a,b);
    }

    //Apply filtre
    if(strcasecmp(argv[1],"filter")==0)
    {
	filters(argv[3]);
    }

    //Apply entropy
    if(strcasecmp(argv[1],"entropy")==0)
    {
	float a,b,c;
	sscanf(argv[3],"%f",&a);
        sscanf(argv[4],"%f",&b);
	sscanf(argv[5],"%f",&c);
	double entropie=entropy(a,b,c);
	MPI_Barrier(MPI_COMM_WORLD);
	if(rank==MASTER)
	{
	    writeDataResidual(outputFileName);
	    printf("\nThe entropy value for the given image: %f\n\n",entropie);
	}

	MPI_Finalize();
	return (EXIT_SUCCESS);
    }


    /************************** END PROCESARE *********************************/

    //Collect information
    if(rank != MASTER )
    {
        printf("[Proces %d] Processing complete. We send the strip to the master process.\n",rank);
        //We copy the data in a buffer, to be transmitted simultaneously

        U8* buffer=(U8*) malloc (stripSize*WIDTH*sizeof(U8));
        for(i=1;i<=stripSize;i++)
            for(j=1;j<=WIDTH;j++)
                buffer[(i-1)*WIDTH + (j-1)]=imageStrip[i][j];
        //Send the data
        MPI_Send(buffer,stripSize*WIDTH,MPI_UNSIGNED_CHAR,MASTER,rank,MPI_COMM_WORLD);
        //Free memory
        free(buffer);
    }
    //MASTER PROCESS
    else
    {
        printf("[MASTER] Processing Complete . We receive the strips from the rest of the processes.\n");
        int sendStripeSize=HEIGHT/ numberProcesses;
        int initialStripSize=sendStripeSize;
        pozCerere=0;
        //We copy the data from each process to the appropriate location in fullImage

        for(i=0;i<numberProcesses;i++)
            if(i!=MASTER)
            {
                printf("[MASTER] \tWe receive the data from the process %d.\n",i);
                if(i==numberProcesses-1)
                    sendStripeSize = HEIGHT - sendStripeSize * (numberProcesses-1);
                MPI_Irecv (fullImage + i*initialStripSize*WIDTH,sendStripeSize*WIDTH,MPI_UNSIGNED_CHAR,i,i,MPI_COMM_WORLD,&requests[pozCerere++]);                
            }
        //We also put the data of the MASTER process in the imageStrip

        for(i=1;i<=stripSize;i++)
            for(j=1;j<=WIDTH;j++)
            {
                fullImage[rank*WIDTH + (i-1)*WIDTH + (j-1)]=imageStrip[i][j];
            }

        printf("[MASTER]\tWe are waiting to receive the data.\n");
        MPI_Waitall(numberProcesses-1,requests,statuses);
    }

    MPI_Finalize();

    //Final writing of the image

    if(rank==MASTER)
        writeData(outputFileName);



    return (EXIT_SUCCESS);
}

