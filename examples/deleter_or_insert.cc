#include <stdio.h>
#include <string>
#include <iostream>

#include "../src/cuckoohash_map.hh"
#include "../src/city_hasher.hh"

int main() {
	cuckoohash_map<int, std::shared_ptr<std::string>, CityHasher<int> >Table;

	for (int i = 0; i < 11; i++) {
		Table.insert(i, std::make_shared<std::string>(std::string( "hello" + std::to_string(i))));
	}

	std::cout << " find(0-11):" << std::endl;

	for (int i = 0; i < 12; i++) {
		std::shared_ptr<std::string> out;

		if (Table.find(i, out)) {
			std::cout << i << "  " << *out << std::endl;
		}
		else {
			std::cout << i << "  NOT FOUND" << std::endl;
		}
	}

	cuckoo_status_ext status;

	std::cout << " find_or_insert (8-13):" << std::endl;

	for (int i = 8; i < 14; i++) {

		std::shared_ptr<std::string> out = Table.find_or_insert(i, status, std::make_shared<std::string>(std::string("default value when find_or_insert")));
		//It is rare to fail.

		std::cout << i << "  " << *out << std::endl;
	}


	std::cout << " update_or_insert (10-14):" << std::endl;

	for (int i = 8; i < 15; i++) {
		std::shared_ptr<std::string> out = (Table.update_or_insert(i, [](std::shared_ptr<std::string> &str)
		{
			if (str->length() == 6)
			{
				*str = *str + "(updated)";
			}
		}, status, std::make_shared<std::string>(std::string("default value when update_or_insert"))));

		std::cout << i << "  " << *out << std::endl;
	}

	std::cout << "delete_or_insert (9-15):" << std::endl;

	for (int i = 9; i < 16; i++) {
		std::shared_ptr<std::string> out = (Table.delete_or_insert(i, [](std::shared_ptr<std::string> &str)
		{
			struct cuckoo_todo todo;
			todo.to_delete = true;
			todo.to_get = false;

			return todo;
		}, status, std::make_shared<std::string>(std::string("default value when delete_or_insert"))));

		if (status == cuckoo_status_ext::inserted)
			std::cout << i << "  " << *out << std::endl;
		else
			std::cout << i << "  deleted" << std::endl;
	}

	std::cout << " find AGAIN:" << std::endl;

	for (int i = 0; i < 17; i++) {
		std::shared_ptr<std::string> out;

		if (Table.find(i, out)) {
			std::cout << i << "  " << *out << std::endl;
		}
		else {
			std::cout << i << "  NOT FOUND" << std::endl;
		}
	}
}
