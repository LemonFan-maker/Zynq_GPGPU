#include "xil_cache.h"

#include "bench_common.h"
#include "bench_suite.h"

int main()
{
    Xil_DCacheDisable();
    timer_init();
    Xil_Out32(GPU_CTRL, 0);

    xil_printf("\n\r");
    xil_printf("########################################\n\r");
    xil_printf("#  Zynq GPGPU Performance Benchmark    #\n\r");
    xil_printf("########################################\n\r");

    print_summary();
    bench_dma();
    bench_vdma_2d();
    bench_vecadd_run();
    bench_conv2d();
    bench_matmul_run();
    bench_matmul_tiled_run();
    bench_mnist_fc();
    bench_dp4a_smoke();
    bench_dp4a_accumulator();
    bench_dp4a_sustained();

    xil_printf("\n\r########################################\n\r");
    xil_printf("#  Benchmark Complete                  #\n\r");
    xil_printf("########################################\n\r");

    return 0;
}
