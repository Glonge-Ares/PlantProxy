#include <iostream>
#include <map>
#include <time.h>
#include <mutex>
#include "BanList.h"




int BanList::add(uint32_t ip, int ban_time) // time in minutes
{
	mtx.lock();
	
	int pardon_time = time(0) + ban_time*60;
	
	ban_list.insert(std::make_pair<uint32_t,int>((uint32_t)ip, (int)pardon_time));
	
	mtx.unlock();
	return 0;
}


int BanList::remove(uint32_t ip)
{
	mtx.lock();
	
	ban_list.erase(ip);
	
	mtx.unlock();
	
	return 0;
}

int BanList::check(uint32_t ip)
{
	mtx.lock();
	
	auto it = ban_list.find(ip);
	
	mtx.unlock();
	
	if(it != ban_list.end())//Found in ban list
		return it->second;
	else
		return -1;
}
