#define NOMINMAX

#include "framework/cli.hpp"

#pragma comment(lib, "cpueaxh.lib")

int main(int argc, char** argv) {
    return cpueaxh_test::run_cli(argc, argv);
}
