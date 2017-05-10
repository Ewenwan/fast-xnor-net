#include "mnist_wrapper.h"

char* TRAIN_IMAGES = "";
char* TRAIN_LABELS = "";
char* TEST_IMAGES  = "";
char* TEST_LABELS  = "";

int NUM_IMAGES;
int NUM_LABELS;
int IMAGE_ROWS;
int IMAGE_COLS;

int ReverseInt (int i)
{
    unsigned char ch1, ch2, ch3, ch4;
    ch1=i&255;
    ch2=(i>>8)&255;
    ch3=(i>>16)&255;
    ch4=(i>>24)&255;
    return((int)ch1<<24)+((int)ch2<<16)+((int)ch3<<8)+ch4;
}

void read_mnist_images_labels(char* images_path, char* labels_path, tensor* input_tensor, int** labels) {

    FILE* fp_images;
    FILE* fp_labels;
    fp_images = fopen(images_path, "rb");
    fp_labels = fopen(labels_path, "rb");

    if(fp_images && fp_labels) {
        int magic_number_images = 0, magic_number_labels = 0;


        // Read magic number from image file
        fread(&magic_number_images, sizeof(magic_number_images), 1, fp_images);
        magic_number_images = ReverseInt(magic_number_images);
        if(magic_number_images != 2051) 
    	{
    		printf("Invalid MNIST image file!\n");
    	}

    	// Read magic number from label file
    	fread(&magic_number_labels, sizeof(magic_number_labels), 1, fp_labels);
        magic_number_labels = ReverseInt(magic_number_labels);
        if(magic_number_labels != 2049) 
    	{
    		printf("Invalid MNIST label file!\n");
    	}

    	// Read number of images from image file
        fread(&NUM_IMAGES, sizeof(NUM_IMAGES), 1, fp_images);
        NUM_IMAGES = ReverseInt(NUM_IMAGES);

        // Read number of labels from label file
        fread(&NUM_LABELS, sizeof(NUM_LABELS), 1, fp_labels);
        NUM_LABELS = ReverseInt(NUM_LABELS);

        // allocate space for labels
        *labels = malloc(NUM_LABELS * sizeof(int));

        // Read number of rows in each image
        fread(&IMAGE_ROWS, sizeof(IMAGE_ROWS), 1, fp_images);
        IMAGE_ROWS = ReverseInt(IMAGE_ROWS);

        // Read number of cols in each image
        fread(&IMAGE_COLS, sizeof(IMAGE_COLS), 1, fp_images);
        IMAGE_COLS = ReverseInt(IMAGE_COLS);

        // Depth is 1, since only one channel for mnist images
        build_args(input_tensor, IMAGE_COLS, IMAGE_ROWS, 1, NUM_IMAGES);

        // Read pixel bytes
        unsigned char temp_char = 0;
        for (int b = 0; b < NUM_IMAGES; ++b)
        {
        	for (int w = 0; w < IMAGE_COLS; ++w)
        	{
        		for (int h = 0; h < IMAGE_ROWS; ++h)
        		{
        			fread(&temp_char, sizeof(unsigned char), 1 , fp_images);
        			(input_tensor->data)[offset(input_tensor,b,w,h,0)] = ((double)temp_char - 127)/127.0;
        		}
        	}
        }

        for (int i = 0; i < NUM_LABELS; ++i)
        {
        	fread(&temp_char, sizeof(unsigned char), 1 , fp_labels);
        	(*labels)[i] = (int)temp_char;
        }
        
        fclose(fp_images);
        fclose(fp_labels);

    } else {
        printf("Cannot open one of images or labels files!!");
    }
}

void test_mnist_load(tensor t, int* labels, int number)
{
	printf("\nIn test_mnist_load:\n");
	printf("-------------------IMAGES:\n");
	printf("BY RAW INDEX\n");
    for (int i = (number-1)*28*28; i < (number-1)*28*28 + 28*7; ++i)
    {
    	if(t.data[i] != 0)
    	printf("t.data[%d]=%f\n", i, t.data[i]);
    }

    printf("\nBY OFFSET\n");
    for (int w = 0; w < 7; ++w)
    {
    	for (int h = 0; h < 28; ++h)
    	{
    		if(t.data[offset(&t,number-1,w,h,0)] != 0)
    		printf("t.data[%d]=%f\n",offset(&t,number-1,w,h,0), t.data[offset(&t,number-1,w,h,0)]);
    	}
    }

    printf("-------------------LABELS:\n");
    for (int i = (number-10); i < number; ++i)
    {
    	if(labels[i] != 0)
    	printf("labels[%d]=%d\n", i, labels[i]);
    }

    printf("\ntest_mnist_load done!\n\n");
}

void test_reverse_int()
{
	printf("\nIn test_reverse_int:\n\n");
	printf("Size of int=%u\n", sizeof(int));
    int i = 4;

    PRINT_INT_TO_BINARY(i)

    printf("i in big endian=%d\n", i);
    
    i = ReverseInt(i);
    PRINT_INT_TO_BINARY(i)
    printf("i in little endian=%d\n", i);
    printf("test_reverse_int done!\n\n");
}

void set_paths()
{
	char cwd[1024];
   if (getcwd(cwd, sizeof(cwd)) != NULL)
       printf("Current working dir: %s\n", cwd);

   char* c = cwd;

   TRAIN_IMAGES = malloc(1024*sizeof(char));
   TRAIN_LABELS = malloc(1024*sizeof(char));
   TEST_IMAGES  = malloc(1024*sizeof(char));
   TEST_LABELS  = malloc(1024*sizeof(char));

   int i = 0;
   while(*c != '\0')
   {
		if (*c == '/')
		{
			TRAIN_IMAGES[i] = *c;
			TRAIN_IMAGES[i+1] = *c;

			TRAIN_LABELS[i] = *c;
			TRAIN_LABELS[i+1] = *c;

			TEST_IMAGES[i] = *c;
			TEST_IMAGES[i+1] = *c;

			TEST_LABELS[i] = *c;
			TEST_LABELS[i+1] = *c;

			i += 2;
		}
		else
		{
			TRAIN_IMAGES[i] = *c;

			TRAIN_LABELS[i] = *c;

			TEST_IMAGES[i] = *c;

			TEST_LABELS[i] = *c;

			i++;
		}

		c++;
   }

   c = "//data//train-images.idx3-ubyte";
   int j = i;
   while(*c != '\0'){
   	TRAIN_IMAGES[j++] = *c;
   	c++;
   }
   TRAIN_IMAGES[j] = '\0';

   c = "//data//train-labels.idx1-ubyte";
   j = i;
   while(*c != '\0'){
   	TRAIN_LABELS[j++] = *c;
   	c++;
   }
   TRAIN_LABELS[j] = '\0';

   c = "//data//t10k-images.idx3-ubyte";
   j = i;
   while(*c != '\0'){
   	TEST_IMAGES[j++] = *c;
   	c++;
   }
   TEST_IMAGES[j] = '\0';

   c = "//data//t10k-labels.idx1-ubyte";
   j = i;
   while(*c != '\0'){
   	TEST_LABELS[j++] = *c;
   	c++;
   }

   TEST_LABELS[j] = '\0';

   printf("\nTrain image file: %s\n", TRAIN_IMAGES);
   printf("Train label file: %s\n", TRAIN_LABELS);
   printf("Test image file: %s\n", TEST_IMAGES);
   printf("Test label file: %s\n\n", TEST_LABELS);
}