#include "lockfreeQueue.h"

#include <iostream>
#include <string>


template<typename Q, typename T>
bool testInterface(const T& origVal)
{
	std::cout << __FUNCTION__ << " Test : interfaces " << typeid(T).name() << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	bool res = true;
	{
		Q q;
		q.push(T(origVal)); //move

		T v;
		q.pop(v); // move

		q.push(v); // copy
		q.push(std::move(v)); // move

		T v2, v3;
		q.pop(v2);
		q.pop(v3);
		if (v3 != v2 || v2 != origVal)
		{
			std::cout << __FUNCTION__ << ":" << __LINE__ << " poped vals don't match the mushed ones: " 
				<< origVal << ","
				<< v2 << "," 
				<< v3 << std::endl;
			return false;
		}
	}

	return res;
}


struct node
{
	node(const std::string& d = "") : data(d) {}
	std::string data;

	bool operator==(const node& rHnd)
	{
		return data == rHnd.data;
	}
	bool operator!=(const node& rHnd)
	{
		bool res = (*this == rHnd);
		return !res;
	}
};
std::ostream& operator<<(std::ostream& s, const node& n)
{
	s << "node " << n.data;
	return s;
}


int main(int argc, char* argv[])
{
	if (!testInterface<concurency::m2oQueue<int, 12, 0>, int>(1))
		return __LINE__;
	if (!testInterface<concurency::m2oQueue<std::string, 12, 0>, std::string>("1"))
		return __LINE__;
	if (!testInterface<concurency::m2oQueue<node, 12, 0>, node>(node("1")))
		return __LINE__;
	return 0;
}