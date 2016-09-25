/* We demonstrate how to nest hash tables within one another, to store
 * unstructured data, kind of like JSON. There's still the limitation that it's
 * statically typed. */

#include <iostream>
#include <string>
#include <memory>
#include <utility>

#include "../src/cuckoohash_map.hh"

typedef cuckoohash_map<std::string, std::string> InnerTable;
typedef cuckoohash_map<std::string, std::unique_ptr<InnerTable>> OuterTable;

void print_all(OuterTable& tbl)
{
	auto lt = tbl.lock_table();
	for (const auto& item : lt) {
		std::cout << "Properties for " << item.first << std::endl;
		auto innerLt = item.second->lock_table();
		for (auto innerItem : innerLt) {
			std::cout << "\t" << innerItem.first << " = "
				<< innerItem.second << std::endl;
		}
	}
}

int main() {
	OuterTable tbl;

	tbl.insert("bob",  std::unique_ptr<InnerTable>(new InnerTable));
	tbl.update_fn("bob", []( std::unique_ptr<InnerTable>& innerTbl) {
		innerTbl->insert("nickname", "jimmy");
		innerTbl->insert("pet", "dog");
		innerTbl->insert("food", "bagels");
	});

	tbl.insert("jack",  std::unique_ptr<InnerTable>(new InnerTable));
	tbl.update_fn("jack", []( std::unique_ptr<InnerTable>& innerTbl) {
		innerTbl->insert("friend", "bob");
		innerTbl->insert("activity", "sleeping");
		innerTbl->insert("language", "javascript");
	});

	std::cout << "\n=======================\nOriginal Data" << std::endl;
	print_all(tbl);

	tbl.update_fn("jack", []( std::unique_ptr<InnerTable>& innerTbl) {
		innerTbl->erase_fn("friend", [](std::string str) {
			if (str == "bob")
				return true;
			else
				return false;
		});

		innerTbl->erase_fn("activity", [](std::string str) {
			if (str == "bob")
				return true;
			else
				return false;
		});
	});

	std::cout << "\n=======================\nAfter Erase ""bob"" pair at inner table" << std::endl;
	print_all(tbl);


	tbl.erase_fn("jack", []( std::unique_ptr<InnerTable>& innerTbl) {
		std::string out;
		if (innerTbl->find("activity", out))
			return true;
		else
			return false;
	});

	std::cout << "\n=======================\nAfter Erase ""activity"" pair at outer table" << std::endl;
	print_all(tbl);
}
