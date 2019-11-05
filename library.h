#ifndef TEST_ID_R_D_LIBRARY_H
#define TEST_ID_R_D_LIBRARY_H


#include "K_means_processor.h"

std::unique_ptr<K_means_processor>
    make_k_means_processor(
        const std::string& config_filename);

#endif //TEST_ID_R_D_LIBRARY_H