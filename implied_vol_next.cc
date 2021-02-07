// To compile for OSX: g++ -o iv implied_volatility.cc -L./osx -lrecipes
// To compile for Linux: g++ -o iv implied_volatility.cc -L./linux -lrecipes -std=c++11
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <limits>
#include <cstddef>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "fin_recipes.h"

void split(const std::string &s, char delim, std::vector<string> &elems)
{
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (getline(ss, item, delim))
  {
    elems.push_back(item);
  }
}

std::vector<string> split(const std::string &s, char delim)
{
  std::vector<string> elems;
  split(s, delim, elems);
  return elems;
}

// Returns number of days since civil 1970-01-01.  Negative values indicate
//    days prior to 1970-01-01.
// Preconditions:  y-m-d represents a date in the civil (Gregorian) calendar
//                 m is in [1, 12]
//                 d is in [1, last_day_of_month(y, m)]
//                 y is "approximately" in
//                   [numeric_limits<Int>::min()/366, numeric_limits<Int>::max()/366]
//                 Exact range of validity is:
//                 [civil_from_days(numeric_limits<Int>::min()),
//                  civil_from_days(numeric_limits<Int>::max()-719468)]
template <class Int>
Int days_from_civil(Int y, unsigned m, unsigned d)
{
  static_assert(std::numeric_limits<unsigned>::digits >= 18,
                "This algorithm has not been ported to a 16 bit unsigned integer");
  static_assert(std::numeric_limits<Int>::digits >= 20,
                "This algorithm has not been ported to a 16 bit signed integer");
  y -= m <= 2;
  const Int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);           // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
  return era * 146097 + static_cast<Int>(doe) - 719468;
}

int get_date_diff(std::string start, std::string end)
{
  std::vector<string> sfields = split(start, '/');
  int sm = std::stoi(sfields[0]);
  int sd = std::stoi(sfields[1]);
  int sy = std::stoi(sfields[2]);

  std::vector<string> efields = split(end, '/');
  int em = std::stoi(efields[0]);
  int ed = std::stoi(efields[1]);
  int ey = std::stoi(efields[2]);

  return (days_from_civil(ey, em, ed) - days_from_civil(sy, sm, sd));
}

// S: Underlying spot price
// K: Strike price
// r: Risk-free rate
// time: time until expiry in years
// C: option market price
void test_black_scholes_implied_volatility(double S, double K, double r, double t, double C)
{
  std::cout << " Black Scholes implied volatility using Newton search = ";
  std::cout << 100.0 * option_price_implied_volatility_call_black_scholes_newton(S, K, r, t, C) << std::endl;
  std::cout << " Black Scholes implied volatility using bisections = ";
  std::cout << 100.0 * option_price_implied_volatility_call_black_scholes_bisections(S, K, r, t, C) << std::endl;
}

int main(int argc, char *argv[])
{
  ifstream fin;
  string dir, filepathdir, filepath, tmpfilepath, outfilepath;
  int num;
  DIR *dpd, *dpf;
  struct dirent *dirpd, *dirpf;
  struct stat filestat;
  struct stat dirstat;

#if 0
	if (argc != 3) {
		std::cout << "Usage : " << argv[0] << " <input file> <output file path>" << std::endl;
		return -1;
	}
#endif

  // dir="/media/sf_IBPYproject/OptionData";
  dir = argv[1];
  dpd = opendir(dir.c_str());
  if (dpd == NULL)
  {
    cout << "Error(" << errno << ") opening " << dir << endl;
    return errno;
  }
  else
  {
    cout << "opened directory" << endl;
  }

  /* If directory open directory */
  // If file open file and open an output file to dump
  // all calculations.

  while ((dirpd = readdir(dpd)))
  {
    filepathdir = "";
    filepathdir = dir + "/" + dirpd->d_name;
    cout << "File path " << filepathdir << endl;
    // If the file is in some way invalid we'll skip it
    if (stat(filepathdir.c_str(), &dirstat) == -1)
    {
      cout << "Invalid file" << endl;
      continue;
    }
    if (S_ISDIR(dirstat.st_mode))
    {
      cout << "ISDIR is not null" << endl; 
    }
    else {
      cout << "ISDIR is NULL" << endl; 
    }
    if (S_ISDIR(dirstat.st_mode) && strstr(filepathdir.c_str(), "..") == NULL)
    {
      dpf = opendir(filepathdir.c_str());
      if (dpf == NULL)
      {
        cout << "Error(" << errno << ") opening " << filepathdir << endl;
        return errno;
      }
      while ((dirpf = readdir(dpf)))
      {
        cout << "Reading the dir now " << endl;
        filepath = "";
        filepath = filepathdir + "/" + dirpf->d_name;
        if (stat(filepath.c_str(), &filestat) == -1)
        {
          cout << "Continue 1" << endl;
          continue;
        }

        if (!S_ISREG(filestat.st_mode))
        {
          cout << "Continue 2" << endl;
          continue;
        }
        /* Skip all files other than .csv files */
        if (strstr(filepath.c_str(), ".csv") == NULL)
        {
          cout << "Continue 3" << endl;
          continue;
        }
        /* Skip if this is a -greeks.csv file  as it is already a processed file */
        if (strstr(filepath.c_str(), "-greeks.csv") != NULL)
        {
          cout << "Continue 4" << endl;
          continue;
        }
        
        /* If output file already exists skip */
        std::size_t found = filepath.find_last_of(".");
        outfilepath = filepath.substr(0, found) + "-greeks.csv";
        if (access(outfilepath.c_str(), F_OK) != -1)
        {
          cout << "Continue 5" << endl;
          continue;
        }
        std::cout << "Input filename: " << filepath;
        std::cout << "  Output filename: " << outfilepath << endl;
        std::ifstream infile(filepath);
        std::ofstream outfile(outfilepath, ios::trunc);
        if (!infile.is_open() || !outfile.is_open())
        {
          std::cout << "Unable to open one of the files" << std::endl;
          return -2;
        }

        // eat the header
        std::string inheader;
        std::getline(infile, inheader);
        std::string null = ",";

        inheader.replace(inheader.end() - 1, inheader.end(), null);
        outfile << inheader << "ImpliedVolatility,Delta,Gamma,Vega,Theta,Rho"
                << "\n";

        std::string line;
        while (std::getline(infile, line))
        {
          std::vector<string> fields = split(line, ',');
          double S = std::stod(fields[1]);
          double K = std::stod(fields[8]);
          double r = 1.0 / 100.0;
          int t = get_date_diff(fields[7], fields[6]);
          double C = 0.5 * (std::stod(fields[10]) + std::stod(fields[11]));
          double iv, iv_newton;
          
          double delta,gamma,vega,rho,theta;
          
          if (fields[5].compare("call") == 0)
          {
            iv = option_price_implied_volatility_call_black_scholes_bisections(S, K, r, double(t) / 365.0, C);
            
            option_price_partials_call_black_scholes(S, K, r, iv, double(t) / 365.0, delta,gamma,theta,vega,rho);
            
          }
          else if (fields[5].compare("put") == 0)
          {
            iv = option_price_implied_volatility_put_black_scholes_bisections(S, K, r, double(t) / 365.0, C);
            
            option_price_partials_put_black_scholes(S, K, r, iv, double(t) / 365.0, delta,gamma,theta,vega,rho);
            
          }
          else
          {
            std::cout << "Error: unexpected sixth field: [" << fields[5] << "]" << std::endl;
            return -3;
          }

          line.replace(line.end() - 1, line.end(), null);
          outfile << line << 100.0 * iv << "," << delta << ","
              << gamma << "," << vega << "," << theta << "," << rho << "\n";
        }

        infile.close();
        outfile.close();
      }
      closedir(dpf);
    } // if directory
    else 
    {
      cout << "Never made it to the big if " << endl;
    }
  }
  closedir(dpd);
}
