#include <stdio.h>
#include <string>
#include <iostream>

#include "../src/cuckoohash_map.hh"
#include "../src/city_hasher.hh"

int main() {
	cuckoohash_map<int,std::shared_ptr<std::string>, CityHasher<int> >Table;

	for (int i = 0; i < 11; i++) {
		Table[i] =std::make_shared<std::string>(std::string("hello" + std::to_string(i)));
	}

	std::cout << " find:" << std::endl;

	for (int i = 0; i < 12; i++) {
		std::shared_ptr<std::string> out;

		if (Table.find(i, out)) {
			std::cout << i << "  " << *out << std::endl;
		}
		else {
			std::cout << i << "  NOT FOUND" << std::endl;
		}
	}

	std::cout << " find_fn:" << std::endl;

	for (int i = 0; i < 12; i++) {
		std::shared_ptr<std::string> out;

		if (Table.find_fn(i, out, [](std::shared_ptr<std::string> &str)
		{
			if (str->length() == 6)
			{
				*str = *str + "(length==6)";//Yes, update is possible in checker!

				return true;
			}
			else
			{
				return false;
			}
		}))
		{
			std::cout << i << "  " << *out << std::endl;
		}
		else
		{
			std::cout << i << "  NOT FOUND" << std::endl;
		}
	}


	std::cout << " find AGAIN:" << std::endl;

	for (int i = 0; i < 12; i++) {
		std::shared_ptr<std::string> out;

		if (Table.find(i, out)) {
			std::cout << i << "  " << *out << std::endl;
		}
		else {
			std::cout << i << "  NOT FOUND" << std::endl;
		}
	}
}
