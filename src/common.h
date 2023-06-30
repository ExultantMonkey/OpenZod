#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>
#include <stdlib.h>

namespace COMMON
{
	//structs
	class xy_struct
	{
	public:
		xy_struct() { x=y=0; }
		xy_struct(int x_, int y_) { x=x_; y=y_; }

		int x, y;
	};

	extern void split(char *dest, char *message, char split, int *initial, int d_size, int m_size);
	extern void clean_newline(char *message, int size);
	extern void lcase(char *message, int m_size);
	extern void lcase(std::string &message);
	extern double current_time();
	extern void uni_pause(int m_sec);
	extern bool good_user_char(int c);
	extern bool good_user_string(const char *message);
	extern void printd_reg(char *message);
	extern std::string data_to_hex_string(unsigned char *data, int size);
	extern bool file_can_be_written(char *filename);
	extern std::vector<std::string> directory_filelist(std::string foldername);
	extern void parse_filelist(std::vector<std::string> &filelist, std::string extension);
	extern bool sort_string_func (const std::string &a, const std::string &b);

	inline void swap(int &a, int &b)
	{
		int c;

		c = a;
		a = b;
		b = c;
	}
	
	inline int xy_to_i(int x, int y, int height) { return x * height + y; }
};

#endif
