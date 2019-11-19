#include <iostream>
#include <random>
#include <fstream>

#include "../K_means_lib.h"

int main()
{

  if constexpr (false)
  {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<double> uniform_distribution(-1000.0, 1000.0);

    std::ofstream output_stream("data.txt", std::ios::out);
    for (int i = 0; i < 40000; ++i)
    {
      for (int j = 0; j < 31; ++j)
      {
        output_stream << uniform_distribution(generator) << " ";
      }

      output_stream << uniform_distribution(generator) << "\n";
      if (i % 10000 == 0)
      {
        std::cout << "10k points: " << i << std::endl;
      }
    }
  }


  auto start_time = std::chrono::steady_clock::now();
  auto processor = K_means_lib::make_k_means_processor("config.txt");
  if (!processor)
  {
    std::cerr << "Error!" << std::endl;
    return 1;
  }

  std::cout << "File reading time (ms): "
    << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() << std::endl;

  start_time = std::chrono::steady_clock::now();
  processor->start();
  auto result = processor->get_result();
  std::cout << "Processing time (ms): "
    << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() << std::endl;
  K_means_lib::print_result_to_file("out.txt", result);

  return 0;
}