#include "main.h"
#include "perf.h"

int NUM_IMAGES;// 图像数量
int IMAGE_ROWS;// 行 680
int IMAGE_COLS;// 列 480

int N_ROWS_CONV;//卷积核尺寸？
int N_COLS_CONV;
int N_ROWS_POOL;//池化核尺寸？
int N_COLS_POOL;
int NUM_TRAIN;// 训练次数
int N_BATCHES;
int TOTAL_FLOPS;// 总的运算量
int NET_TYPE;// 网络类型

int* shuffle_index;

// dimension: 60k*28*28
tensor input_images;// mnist 手写字体 图像尺寸 28*28
int* labels;// 标签

tensor fil_w;// 卷积核
tensor fil_b;// 偏置项

// dimension: BATCH_SIZE*24*24
tensor conv_t;

// dimension: BATCH_SIZE*12*12
tensor pool_t;

int**** pool_index_i;// 四级指针!!!!!!!!!!!!!!!!!!!!
int**** pool_index_j;

tensor fully_con_w, fully_con_b, fully_con_out;
tensor softmax_out;
tensor del_max_pool;
tensor del_conv;

// binarized input batch of images
int*** bin_input_images;// 图像的二值输入
double*** betas;

// binarized weights in conv layer 二值权重矩阵
int fil_bin_w[NUM_FILS][FIL_ROWS][FIL_COLS];
double alphas[NUM_FILS];

// accuracy on taining set
int preds[BATCH_SIZE];
double train_acc = 0.0;
int correct_preds = 0;

// accuracy on validation set
double val_acc = 0.0;

// perf counters
double binarize_cycles = 0;
double bin_activ_cycles = 0;
double conv_cycles = 0;
double pool_cycles = 0;
double fully_cycles = 0;
double soft_cycles = 0;
double bp_soft_to_pool_cycles = 0;
double bp_pool_to_conv_cycles = 0;
double forward_cycles = 0;
double backward_cycles = 0;
double total_cycles = 0;

double bp_softmax_to_conv_cycles = 0;
double update_softmax_weights_cycles = 0;
double update_softmax_biases_cycles = 0;
double update_conv_weights_cycles = 0;
double update_conv_biases_cycles = 0;

int main(int argc, char* argv[]){

    printf("starting program\n");

    if (argc < 2)
    {
      printf("Please enter the net type(0->NORMAL_NET, 1->BINARY_NET, 2->XNOR_NET) which you want to run\n");
      return;
    }

    NET_TYPE = argv[1][0] - '0';// 第二个域(字符串)的第一个字符
    printf("%d\n", NET_TYPE);

    if (NET_TYPE == 0)
    {
      TOTAL_FLOPS = NORMAL_FLOPS;// 全精度
    }
    else if (NET_TYPE == 1)
    {
      TOTAL_FLOPS = BINARY_FLOPS;// 权重二值
    }
    else if (NET_TYPE == 2)
    {
      TOTAL_FLOPS = XNOR_FLOPS;  // 权重和输入都二值
    }
    else
    {
      printf("Wrong net type entered. Please enter (0->NORMAL_NET, 1->BINARY_NET, 2->XNOR_NET)\n");
      return;
    }

    perf_init();// IntelPCM is about to be initialized  Performance Counter Monitor(性能计数器监视器 PCM) 
  // 禁用NMI echo 0 > /proc/sys/kernel/nmi_watchdog   开启 PCM
  // 开启 msr的使用权限  /dev/cpu/*/msr
  // MSR全称是“Model-Specific Register（粗暴翻译为：型号特征注册器）”，
  // 是X86平台提供给Linux Kernel的一个用户级别的接口，
  // 旨在提供一个方便用户对系统进行debugging，
  // 执行跟踪以及进一步的性能监控的可靠功能。
  // 如果你之前使用过perf_event库，或者Intel PCM 库，你应该接触到msr。
  // 这个模块默认系统是不加载的，如果需要启动此功能，需要首先使用如下命令加载模块：
  // modprobe msr
  // 这个时候如果访问/dev/cpu/ 目录，应该会有若干个以数字命名的目录，每一个目录都映射到CPU的一个core上。
  // 开启权限 chmod 777 /dev/cpu/*/msr
  
  // Performance Counter Monitor（PCM）是一个由英特尔开发的，
  // 也是基于PMU(performance monitoring unit， 性能监视单元，其实CPU提供的一个单元，属于硬件的范畴)一个性能检测工具。
它// 是运行在msr 内核模块(perf是内核系统调用吧？)上的，提供了C++ API。
  // 通过访问相关的寄存器能读取到CPU的一些性能数据，
  // 目前大部分CPU都会提供相应的PMU，下面主要学习Intel系列CPU的PMU。
  
  
  // 使用性能计数器之前，应首先对它们进行初始化。
  // 完成初始化后，可在相关代码段之前和之后，也就是两个时间点捕获计数器状态。
  // 不同例程会捕获面向内核、插槽或整个系统的计数器，并将它们的状态存储于相应的数据结构中。
  // 其它例程则可基于这些状态计算性能指标。
  
    #ifdef COUNT_FLOPS
      TOTAL_FLOPS = 0;// 记录 运算量
    #endif

    //test_tensor();
    //test_reverse_int();

    set_paths();
 
// 读取手写字体数据集
    read_mnist_images_labels(TRAIN_IMAGES, TRAIN_LABELS, &input_images, &labels);

    printf("number_of_images=%d\n", NUM_IMAGES);
    printf("number_of_labels=%d\n", NUM_IMAGES);
    printf("n_rows in each image=%d\n", IMAGE_ROWS);
    printf("n_cols in each image=%d\n\n", IMAGE_COLS);

    //test_mnist_load(input_images, labels, NUM_IMAGES);

    // Now we have input layer as input_images. Next is convolution layer


    build_args(&fil_w, FIL_COLS, FIL_ROWS, FIL_DEPTH, NUM_FILS);
    build_args(&fil_b, 1, 1, 1, NUM_FILS);
    initialize_filters(&fil_w, &fil_b);
    print_filters(&fil_w, &fil_b);

    N_ROWS_CONV = IMAGE_ROWS - FIL_ROWS + 1;
    N_COLS_CONV = IMAGE_COLS - FIL_COLS + 1;
    // dimension: BATCH_SIZE*24*24
    build_args(&conv_t, N_COLS_CONV, N_ROWS_CONV, NUM_FILS, BATCH_SIZE);


    N_ROWS_POOL = N_ROWS_CONV/POOL_DIM;
    N_COLS_POOL = N_COLS_CONV/POOL_DIM;

    int index_i[BATCH_SIZE][NUM_FILS][N_ROWS_POOL][N_COLS_POOL];
    int index_j[BATCH_SIZE][NUM_FILS][N_ROWS_POOL][N_COLS_POOL];

    pool_index_i = index_i;
    pool_index_j = index_j;

    // dimension: BATCH_SIZE*12*12
    build_args(&pool_t, N_COLS_POOL, N_ROWS_POOL, NUM_FILS, BATCH_SIZE);

    //全链接层  fully connected layer
    build_args(&fully_con_w, N_COLS_POOL, N_ROWS_POOL, NUM_FILS, N_DIGS);
    build_args(&fully_con_b, 1, 1, N_DIGS, 1);
    build_args(&fully_con_out, 1, 1, N_DIGS, BATCH_SIZE);

    // 归一化分类层 softmax layer
    build_args(&softmax_out, 1, 1, N_DIGS, BATCH_SIZE);
    // 初始化卷积权重和偏置项
    initialize_weights_biases(&fully_con_w, &fully_con_b);

    // backprop to max-pool layer
    build_args(&del_max_pool, N_COLS_POOL, N_ROWS_POOL, NUM_FILS, BATCH_SIZE);

    // backprop to conv layer
    build_args(&del_conv, N_COLS_CONV, N_ROWS_CONV, NUM_FILS, BATCH_SIZE);

    NUM_TRAIN = NUM_IMAGES - NUM_VAL;
    N_BATCHES = COUNT_BATCHES;

    // First 50k indexes for training, next 10k indexes for validation
    shuffle_index = malloc(NUM_IMAGES*sizeof(int));

    if (NET_TYPE == 0)
    {
        normal_net();// 全精度网络
    }
    else if (NET_TYPE == 1)
    {
        binary_net();// 权重二值化网络
    }
    else
    {
        int bin_array[BATCH_SIZE][IMAGE_ROWS][IMAGE_COLS];
        bin_input_images = bin_array;// 输入也二值化

        double betas_array[BATCH_SIZE][N_ROWS_CONV][N_COLS_CONV];
        betas = betas_array;

        // pre-calculate sign(I)
        //binarize_input(&input_images, &bin_input_images);

        xnor_net();// 权重+ 输入 二值化网络
    }

    // testing convolution with last image in last batch
    //print_tensor(&input_images, shuffle_index[0], 28);
    //print_tensor(&conv_t, 0, 24);
    //print_tensor(&pool_t, 0, 12);

    //print_pool_mat(pool_index_i, pool_index_j, 99);

    //print_tensor_1d(&fully_con_out, 10, 99);
    //print_tensor_1d(&softmax_out, 10, 99);
    
    //  释放内存
    destroy(&input_images);
    destroy(&conv_t);
    destroy(&pool_t);
    destroy(&fully_con_w);
    destroy(&fully_con_b);
    destroy(&fully_con_out);
    destroy(&softmax_out);
    destroy(&del_conv);
    destroy(&del_max_pool);

    return 0;
}

// 全精度网络
void normal_net()
{ //  #define NUM_EPOCHS 20 // 训练20次 定义在 common.h中
    for (int epoch = 0; epoch < NUM_EPOCHS; ++epoch)
    {
        // Shuffle all 60k only once, they keep last 10k for validtion
        initialize_cycle_counter();
        if (epoch == 0)
        {
            shuffle(shuffle_index, NUM_IMAGES);
        }
        else
        {
            shuffle(shuffle_index, NUM_TRAIN);
        }

        correct_preds = 0;
        for (int i = 0; i < N_BATCHES; ++i)
        {

            cycles_count_start();
    	    convolution(&input_images, &conv_t, BATCH_SIZE, &fil_w, &fil_b, i*BATCH_SIZE, shuffle_index);
            conv_cycles += cycles_count_stop();

            cycles_count_start();
    	    max_pooling(&conv_t, &pool_t, pool_index_i, pool_index_j, BATCH_SIZE, 'T');
            pool_cycles += cycles_count_stop();

            cycles_count_start();
    	    feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, BATCH_SIZE);
            fully_cycles += cycles_count_stop();

            cycles_count_start();
    	    softmax(&fully_con_out, &softmax_out, preds, BATCH_SIZE);
            soft_cycles += cycles_count_stop();

            cycles_count_start();
    	    bp_softmax_to_maxpool(&del_max_pool, &softmax_out, labels, i*BATCH_SIZE, &fully_con_w, shuffle_index);
          bp_soft_to_pool_cycles += cycles_count_stop();

          cycles_count_start();
    	    update_sotmax_weights(&fully_con_w, &softmax_out, &pool_t, labels, i*BATCH_SIZE, shuffle_index);
          update_softmax_weights_cycles += cycles_count_stop();

          cycles_count_start();
    	    update_sotmax_biases(&fully_con_b, &softmax_out, labels, i*BATCH_SIZE, shuffle_index);
          update_softmax_biases_cycles += cycles_count_stop();

            cycles_count_start();
    	    bp_maxpool_to_conv(&del_conv, &del_max_pool, &conv_t, pool_index_i, pool_index_j);
          bp_pool_to_conv_cycles += cycles_count_stop();
            // update weights and biases
          cycles_count_start();
    	    update_conv_weights(&fil_w, &del_conv, &conv_t, &input_images, i*BATCH_SIZE, shuffle_index);
          update_conv_weights_cycles += cycles_count_stop();

          cycles_count_start();
    	    update_conv_biases(&fil_b, &del_conv, &conv_t);
          update_conv_biases_cycles += cycles_count_stop();


            correct_preds += calc_correct_preds(preds, labels, i, shuffle_index);

            reset_to_zero(&del_max_pool);
            reset_to_zero(&del_conv);
            reset_to_zero(&conv_t);
            reset_to_zero(&pool_t);
            reset_to_zero(&fully_con_out);
            reset_to_zero(&softmax_out);
        }
        #ifndef COUNT_FLOPS
           train_acc = (correct_preds*100.0) / NUM_TRAIN;
           val_acc = validate();
           printf("NetType=%d, Epoch=%3d, train_acc=%f val_acc=%f \n",
                               NET_TYPE, epoch+1, train_acc, val_acc);
       #endif

       binarize_cycles /= N_BATCHES;
       conv_cycles     /= N_BATCHES;
       pool_cycles     /= N_BATCHES;
       fully_cycles    /= N_BATCHES;
       soft_cycles     /= N_BATCHES;

       forward_cycles = (binarize_cycles + conv_cycles + pool_cycles + fully_cycles + soft_cycles);


       bp_softmax_to_conv_cycles     /= N_BATCHES;
       bp_soft_to_pool_cycles        /= N_BATCHES;
       bp_pool_to_conv_cycles        /= N_BATCHES;
       update_softmax_weights_cycles /= N_BATCHES;
       update_softmax_biases_cycles  /= N_BATCHES;
       update_conv_weights_cycles    /= N_BATCHES;
       update_conv_biases_cycles     /= N_BATCHES;

       backward_cycles = (bp_softmax_to_conv_cycles + bp_soft_to_pool_cycles + bp_pool_to_conv_cycles
                       + update_softmax_weights_cycles + update_softmax_biases_cycles
                       + update_conv_weights_cycles + update_conv_biases_cycles);

       total_cycles = forward_cycles + backward_cycles;

       printf("\nPER BATCH STATS\n");
       printf("EPOCH                 : %d\n\n", epoch+1);
       printf("binarize_cycles       : %.2f\n", binarize_cycles);
       printf("conv_cycles           : %.2f\n", conv_cycles);
       printf("pool_cycles           : %.2f\n", pool_cycles);
       printf("fully_cycles          : %.2f\n", fully_cycles);
       printf("soft_cycles           : %.2f\n\n", soft_cycles);

       printf("bp_soft_to_conv       : %.2f\n", bp_softmax_to_conv_cycles);
       printf("bp_soft_to_pool       : %.2f\n", bp_soft_to_pool_cycles);
       printf("bp_pool_to_conv       : %.2f\n", bp_pool_to_conv_cycles);
       printf("update_softmax_weights: %.2f\n", update_softmax_weights_cycles);
       printf("update_softmax_biases : %.2f\n", update_softmax_biases_cycles);
       printf("update_conv_weights   : %.2f\n", update_conv_weights_cycles);
       printf("update_conv_biases    : %.2f\n\n", update_conv_biases_cycles);

       printf("forward_cycles        : %.2f\n", forward_cycles);
       printf("backward_cycles       : %.2f\n", backward_cycles);
       printf("total_cycles          : %.2f\n", total_cycles);
       printf("\n");
       PRINT_FLOPS();
       PRINT_PERF(total_cycles);
    }
}

// 权重二值化网络
void binary_net()
{

    for (int epoch = 0; epoch < NUM_EPOCHS; ++epoch)
    {
      initialize_cycle_counter();
        // Shuffle all 60k only once, they keep last 10k for validtion

        if (epoch == 0)
        {
            shuffle(shuffle_index, NUM_IMAGES);
        }
        else
        {
            shuffle(shuffle_index, NUM_TRAIN);
        }

        correct_preds = 0;
        for (int i = 0; i < N_BATCHES; ++i)
        {
            cycles_count_start();
            binarize_filters(&fil_w, fil_bin_w, alphas);
            binarize_cycles += cycles_count_stop();

            cycles_count_start();
            bin_convolution(&input_images, &conv_t, BATCH_SIZE, fil_bin_w, alphas,
                fil_b, i*BATCH_SIZE, shuffle_index);
            conv_cycles += cycles_count_stop();

            cycles_count_start();
            max_pooling(&conv_t, &pool_t, pool_index_i, pool_index_j, BATCH_SIZE, 'T');
            pool_cycles += cycles_count_stop();

            cycles_count_start();
            feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, BATCH_SIZE);
            fully_cycles += cycles_count_stop();

            cycles_count_start();
            softmax(&fully_con_out, &softmax_out, preds, BATCH_SIZE);
            soft_cycles += cycles_count_stop();

            cycles_count_start();
            bp_softmax_to_maxpool(&del_max_pool, &softmax_out, labels, i*BATCH_SIZE, &fully_con_w, shuffle_index);
            bp_soft_to_pool_cycles += cycles_count_stop();

            cycles_count_start();
            bp_maxpool_to_conv(&del_conv, &del_max_pool, &conv_t, pool_index_i, pool_index_j);
            bp_pool_to_conv_cycles += cycles_count_stop();

            // update weights and biases
            cycles_count_start();
            update_sotmax_weights(&fully_con_w, &softmax_out, &pool_t, labels, i*BATCH_SIZE, shuffle_index);
            update_softmax_weights_cycles += cycles_count_stop();

            cycles_count_start();
            update_sotmax_biases(&fully_con_b, &softmax_out, labels, i*BATCH_SIZE, shuffle_index);
            update_softmax_biases_cycles += cycles_count_stop();

            cycles_count_start();
            update_conv_weights(&fil_w, &del_conv, &conv_t, &input_images, i*BATCH_SIZE, shuffle_index);
            update_conv_weights_cycles += cycles_count_stop();

            cycles_count_start();
            update_conv_biases(&fil_b, &del_conv, &conv_t);
            update_conv_biases_cycles += cycles_count_stop();

            correct_preds += calc_correct_preds(preds, labels, i, shuffle_index);

            reset_to_zero(&del_max_pool);
            reset_to_zero(&del_conv);
            reset_to_zero(&conv_t);
            reset_to_zero(&pool_t);
            reset_to_zero(&fully_con_out);
            reset_to_zero(&softmax_out);
        }
        #ifndef COUNT_FLOPS
           train_acc = (correct_preds*100.0) / NUM_TRAIN;
           val_acc = bin_validate();
           printf("\nNetType=%d, Epoch=%3d, train_acc=%f val_acc=%f \n",
                               NET_TYPE, epoch+1, train_acc, val_acc);
       #endif

       binarize_cycles /= N_BATCHES;
       conv_cycles     /= N_BATCHES;
       pool_cycles     /= N_BATCHES;
       fully_cycles    /= N_BATCHES;
       soft_cycles     /= N_BATCHES;

       forward_cycles = (binarize_cycles + conv_cycles + pool_cycles + fully_cycles + soft_cycles);


       bp_softmax_to_conv_cycles     /= N_BATCHES;
       bp_soft_to_pool_cycles        /= N_BATCHES;
       bp_pool_to_conv_cycles        /= N_BATCHES;
       update_softmax_weights_cycles /= N_BATCHES;
       update_softmax_biases_cycles  /= N_BATCHES;
       update_conv_weights_cycles    /= N_BATCHES;
       update_conv_biases_cycles     /= N_BATCHES;

       backward_cycles = (bp_softmax_to_conv_cycles + bp_soft_to_pool_cycles + bp_pool_to_conv_cycles
                       + update_softmax_weights_cycles + update_softmax_biases_cycles
                       + update_conv_weights_cycles + update_conv_biases_cycles);

       total_cycles = forward_cycles + backward_cycles;

       printf("\nPER BATCH STATS\n");
       printf("EPOCH                 : %d\n\n", epoch+1);
       printf("binarize_cycles       : %.2f\n", binarize_cycles);
       printf("conv_cycles           : %.2f\n", conv_cycles);
       printf("pool_cycles           : %.2f\n", pool_cycles);
       printf("fully_cycles          : %.2f\n", fully_cycles);
       printf("soft_cycles           : %.2f\n\n", soft_cycles);

       printf("bp_soft_to_conv       : %.2f\n", bp_softmax_to_conv_cycles);
       printf("bp_soft_to_pool       : %.2f\n", bp_soft_to_pool_cycles);
       printf("bp_pool_to_conv       : %.2f\n", bp_pool_to_conv_cycles);
       printf("update_softmax_weights: %.2f\n", update_softmax_weights_cycles);
       printf("update_softmax_biases : %.2f\n", update_softmax_biases_cycles);
       printf("update_conv_weights   : %.2f\n", update_conv_weights_cycles);
       printf("update_conv_biases    : %.2f\n\n", update_conv_biases_cycles);

       printf("forward_cycles        : %.2f\n", forward_cycles);
       printf("backward_cycles       : %.2f\n", backward_cycles);
       printf("total_cycles          : %.2f\n", total_cycles);
       printf("\n");
       PRINT_FLOPS();
       PRINT_PERF(total_cycles);
    }
}

// 权重+ 输入 二值化网络
void xnor_net()
{

    for (int epoch = 0; epoch < NUM_EPOCHS; ++epoch)
    {
      initialize_cycle_counter();
        // Shuffle all 60k only once, they keep last 10k for validtion
        if (epoch == 0)
        {
            shuffle(shuffle_index, NUM_IMAGES);
        }
        else
        {
            shuffle(shuffle_index, NUM_TRAIN);
        }

        correct_preds = 0;
        for (int i = 0; i < N_BATCHES; ++i)
        {
            cycles_count_start();
            binarize_filters(&fil_w, fil_bin_w, alphas);
            binarize_cycles += cycles_count_stop();

            // calculate betas
            cycles_count_start();
            bin_activation(&input_images, bin_input_images, shuffle_index, betas, BATCH_SIZE, i*BATCH_SIZE);
            bin_activ_cycles += cycles_count_stop();

            cycles_count_start();
            xnor_convolution(bin_input_images, betas, &conv_t, BATCH_SIZE, fil_bin_w, alphas,
                fil_b, i*BATCH_SIZE, shuffle_index);
            conv_cycles += cycles_count_stop();

            cycles_count_start();
            max_pooling(&conv_t, &pool_t, pool_index_i, pool_index_j, BATCH_SIZE, 'T');
            pool_cycles += cycles_count_stop();

            cycles_count_start();
            feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, BATCH_SIZE);
            fully_cycles += cycles_count_stop();

            cycles_count_start();
            softmax(&fully_con_out, &softmax_out, preds, BATCH_SIZE);
            soft_cycles += cycles_count_stop();

            cycles_count_start();
            bp_softmax_to_maxpool(&del_max_pool, &softmax_out, labels, i*BATCH_SIZE, &fully_con_w, shuffle_index);
            bp_soft_to_pool_cycles += cycles_count_stop();

            cycles_count_start();
            bp_maxpool_to_conv(&del_conv, &del_max_pool, &conv_t, pool_index_i, pool_index_j);
            bp_pool_to_conv_cycles += cycles_count_stop();

            // update weights and biases
            cycles_count_start();
            update_sotmax_weights(&fully_con_w, &softmax_out, &pool_t, labels, i*BATCH_SIZE, shuffle_index);
            update_softmax_weights_cycles += cycles_count_stop();

            cycles_count_start();
            update_sotmax_biases(&fully_con_b, &softmax_out, labels, i*BATCH_SIZE, shuffle_index);
            update_softmax_biases_cycles += cycles_count_stop();

            cycles_count_start();
            update_conv_weights(&fil_w, &del_conv, &conv_t, &input_images, i*BATCH_SIZE, shuffle_index);
            update_conv_weights_cycles += cycles_count_stop();

            cycles_count_start();
            update_conv_biases(&fil_b, &del_conv, &conv_t);
            update_conv_biases_cycles += cycles_count_stop();

            correct_preds += calc_correct_preds(preds, labels, i, shuffle_index);


            reset_to_zero(&del_max_pool);
            reset_to_zero(&del_conv);
            reset_to_zero(&conv_t);
            reset_to_zero(&pool_t);
            reset_to_zero(&fully_con_out);
            reset_to_zero(&softmax_out);
        }
        #ifndef COUNT_FLOPS
           train_acc = (correct_preds*100.0) / NUM_TRAIN;
           val_acc = xnor_validate();
           printf("\nNetType=%d, Epoch=%3d, train_acc=%f val_acc=%f \n",
                               NET_TYPE, epoch+1, train_acc, val_acc);
       #endif

       binarize_cycles /= N_BATCHES;
       bin_activ_cycles/= N_BATCHES;
       conv_cycles     /= N_BATCHES;
       pool_cycles     /= N_BATCHES;
       fully_cycles    /= N_BATCHES;
       soft_cycles     /= N_BATCHES;

       forward_cycles = (binarize_cycles + bin_activ_cycles + conv_cycles + pool_cycles + fully_cycles + soft_cycles);


       bp_softmax_to_conv_cycles     /= N_BATCHES;
       bp_soft_to_pool_cycles        /= N_BATCHES;
       bp_pool_to_conv_cycles        /= N_BATCHES;
       update_softmax_weights_cycles /= N_BATCHES;
       update_softmax_biases_cycles  /= N_BATCHES;
       update_conv_weights_cycles    /= N_BATCHES;
       update_conv_biases_cycles     /= N_BATCHES;

       backward_cycles = (bp_softmax_to_conv_cycles + bp_soft_to_pool_cycles + bp_pool_to_conv_cycles
                       + update_softmax_weights_cycles + update_softmax_biases_cycles
                       + update_conv_weights_cycles + update_conv_biases_cycles);

       total_cycles = forward_cycles + backward_cycles;

       printf("\nPER BATCH STATS\n");
       printf("EPOCH                 : %d\n\n", epoch+1);
       printf("binarize_cycles       : %.2f\n", binarize_cycles);
       printf("bin_activ_cycles      : %.2f\n", bin_activ_cycles);
       printf("conv_cycles           : %.2f\n", conv_cycles);
       printf("pool_cycles           : %.2f\n", pool_cycles);
       printf("fully_cycles          : %.2f\n", fully_cycles);
       printf("soft_cycles           : %.2f\n\n", soft_cycles);

       printf("bp_soft_to_conv       : %.2f\n", bp_softmax_to_conv_cycles);
       printf("bp_soft_to_pool       : %.2f\n", bp_soft_to_pool_cycles);
       printf("bp_pool_to_conv       : %.2f\n", bp_pool_to_conv_cycles);
       printf("update_softmax_weights: %.2f\n", update_softmax_weights_cycles);
       printf("update_softmax_biases : %.2f\n", update_softmax_biases_cycles);
       printf("update_conv_weights   : %.2f\n", update_conv_weights_cycles);
       printf("update_conv_biases    : %.2f\n\n", update_conv_biases_cycles);

       printf("forward_cycles        : %.2f\n", forward_cycles);
       printf("backward_cycles       : %.2f\n", backward_cycles);
       printf("total_cycles          : %.2f\n", total_cycles);
       printf("\n");
       PRINT_FLOPS();
       PRINT_PERF(total_cycles);
    }
}

void print_pool_mat(int mat1[BATCH_SIZE][NUM_FILS][N_ROWS_POOL][N_COLS_POOL], int mat2[BATCH_SIZE][NUM_FILS][N_ROWS_POOL][N_COLS_POOL], int num){
	printf("Max Pooling: %d\n", num);
	for (int i = 0; i < N_ROWS_POOL; ++i)
	{
		for (int j = 0; j < N_COLS_POOL; ++j)
		{
			printf("%2d-%2d, ", mat1[num][0][i][j], mat2[num][0][i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

int calc_correct_preds(int preds[BATCH_SIZE], int* labels, int num_batch, int shuffle_index[]){
    int base = BATCH_SIZE*num_batch;
    int ret = 0;
    for (int i = 0; i < BATCH_SIZE; ++i)
    {
        if (preds[i] == labels[ shuffle_index[base+i] ])
        {
            ret++;
        }
    }

    return ret;
}

// 打乱数据集
void shuffle(int shuffle_index[], int number_of_images){
    srand( time(NULL) );

    for (int i = 0; i < number_of_images; ++i)
    {
        shuffle_index[i] = i;
    }

    for (int i = number_of_images-1; i >= 0; --i){
    //generate a random number [0, n-1]
    int j = rand() % (i+1);

    //swap the last element with element at random index
    int temp = shuffle_index[i];
    shuffle_index[i] = shuffle_index[j];
    shuffle_index[j] = temp;
    }
}

// 全精度网络的 验证 
double validate(){

    int pred[1];
    int correct_preds = 0;
    for (int i = NUM_TRAIN; i < NUM_TRAIN + NUM_VAL; ++i)
        {
            convolution(&input_images, &conv_t, 1, &fil_w, &fil_b, i, shuffle_index);
            max_pooling(&conv_t, &pool_t, NULL, NULL, 1, 'V');
            feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, 1);
            softmax(&fully_con_out, &softmax_out, pred, 1);

            correct_preds += (labels[shuffle_index[i]] == pred[0]);
        }

    return (correct_preds*100.0) / NUM_VAL;
}
// 权重二值网络的 验证
double bin_validate(){

    int pred[1];
    int correct_preds = 0;
    for (int i = NUM_TRAIN; i < NUM_TRAIN + NUM_VAL; ++i)
        {
            bin_convolution(&input_images, &conv_t, 1, fil_bin_w, alphas, fil_b, i, shuffle_index);
            max_pooling(&conv_t, &pool_t, NULL, NULL, 1, 'V');
            feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, 1);
            softmax(&fully_con_out, &softmax_out, pred, 1);

            correct_preds += (labels[shuffle_index[i]] == pred[0]);
        }
    return (correct_preds*100.0) / NUM_VAL;
}
// 权重+输入二值网络的验证
double xnor_validate(){

    int pred[1];
    int correct_preds = 0;
    for (int i = NUM_TRAIN; i < NUM_TRAIN + NUM_VAL; ++i)
        {
            // calculate betas
            bin_activation(&input_images, bin_input_images, shuffle_index, betas, 1, i);

            xnor_convolution(bin_input_images, betas, &conv_t, 1, fil_bin_w, alphas, fil_b, i, shuffle_index);

            max_pooling(&conv_t, &pool_t, NULL, NULL, 1, 'V');
            feed_forward(&pool_t, &fully_con_out, &fully_con_w, &fully_con_b, 1);
            softmax(&fully_con_out, &softmax_out, pred, 1);

            correct_preds += (labels[shuffle_index[i]] == pred[0]);
        }

    return (correct_preds*100.0) / NUM_VAL;
}
// 初始化计数器
initialize_cycle_counter()
{
    binarize_cycles = 0;
    bin_activ_cycles = 0;
    conv_cycles = 0;
    pool_cycles = 0;
    fully_cycles = 0;
    soft_cycles = 0;
    bp_soft_to_pool_cycles = 0;
    bp_pool_to_conv_cycles = 0;
    forward_cycles = 0;
    backward_cycles = 0;
    total_cycles = 0;

    bp_softmax_to_conv_cycles = 0;
    update_softmax_weights_cycles = 0;
    update_softmax_biases_cycles = 0;
    update_conv_weights_cycles = 0;
    update_conv_biases_cycles = 0;
}
