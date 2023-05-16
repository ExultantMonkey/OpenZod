#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>
#include <stdlib.h>

using namespace std;

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
	extern void lcase(string &message);
	extern double current_time();
	extern void uni_pause(int m_sec);
	extern double distance(int x1, int y1, int x2, int y2);
	extern double point_distance_from_line(int x1, int y1, int x2, int y2, int px, int py);
	extern bool points_within_distance(int x1, int y1, int x2, int y2, int distance);
	extern bool points_within_area(int px, int py, int ax, int ay, int aw, int ah);
	extern bool good_user_char(int c);
	extern bool good_user_string(const char *message);
	extern void printd_reg(char *message);
	extern string data_to_hex_string(unsigned char *data, int size);
	extern bool file_can_be_written(char *filename);
	extern vector<string> directory_filelist(string foldername);
	extern void parse_filelist(vector<string> &filelist, string extension);
	extern bool sort_string_func (const string &a, const string &b);

	//inline functions...
	inline bool isz(float num) { return (num < 0.00001 && num > -0.00001); };
	inline bool isz(double num) { return (num < 0.00001 && num > -0.00001); };

	inline bool is1(float num) { return (num < 1.00001 && num > 0.99999); };
	inline bool is1(double num) { return (num < 1.00001 && num > 0.99999); };

	inline void swap(int &a, int &b)
	{
		int c;

		c = a;
		a = b;
		b = c;
	}

	inline double frand() { return (rand()%10001) / 10000.0; }
	
	inline int xy_to_i(int x, int y, int height) { return x * height + y; }
};

#endif
