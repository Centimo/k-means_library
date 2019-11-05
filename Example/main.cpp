#include <iostream>
#include <random>
#include <fstream>

#include "../library.h"

int main()
{

std::random_device rd;
std::mt19937 generator(rd());
std::uniform_real_distribution<float> uniform_distribution(0.0, 10.0);


  {
    std::ofstream output_stream("data.txt", std::ios::out);
    for (int i = 0; i < 150; ++i)
    {
      for (int j = 0; j < 19; ++j)
      {
        output_stream << uniform_distribution(generator) << ", ";
      }

      output_stream << uniform_distribution(generator) << "\n";
    }
  }


  auto processor = K_means_lib::make_k_means_processor("config.txt");
  if (!processor)
  {
    std::cerr << "Error!" << std::endl;
    return 1;
  }

  processor->start();
  K_means_lib::print_result_to_file("out.txt", processor->get_result());

  return 0;
}