cmake_minimum_required(VERSION 3.20)
project(structure_boot_vm_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(QUERY_SRC
    src/struct_path.c
    src/boot.c
    src/claim.c
    src/vmm_type.c)

add_executable(structure_boot_vm_answer main.c ${QUERY_SRC})
add_executable(structure_boot_vm_answer_test test.c ${QUERY_SRC}
               src/mbr_image.c src/guest_vm.c)
add_executable(structure_boot_vm_answer_mbr mbr.c src/mbr_image.c)
add_executable(structure_boot_vm_answer_trap trap.c src/guest_vm.c)

include(../tests/check.cmake OPTIONAL)
