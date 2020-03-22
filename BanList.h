#include <iostream>
#include <map>
#include <time.h>
#include <mutex>

class BanList
{
	private:
		std::map <uint32_t,int> ban_list;
		//int(IP addr), int(unban time in unix format)
		std::mutex mtx;
		
	public:
		int add(uint32_t ip, int ban_time);
		int remove(uint32_t ip);
		int check(uint32_t ip);
};
