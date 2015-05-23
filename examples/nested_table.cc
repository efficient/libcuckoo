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

int main() {
    OuterTable tbl;

    tbl.insert("bob", std::unique_ptr<InnerTable>(new InnerTable));
    tbl.update_fn("bob", [] (std::unique_ptr<InnerTable>& innerTbl) {
            innerTbl->insert("nickname", "jimmy");
            innerTbl->insert("pet", "dog");
            innerTbl->insert("food", "bagels");
        });

    tbl.insert("jack", std::unique_ptr<InnerTable>(new InnerTable));
    tbl.update_fn("jack", [] (std::unique_ptr<InnerTable>& innerTbl) {
            innerTbl->insert("friend", "bob");
            innerTbl->insert("activity", "sleeping");
            innerTbl->insert("language", "javascript");
        });

    for (auto it = tbl.cbegin(); !it.is_end(); it++) {
        std::cout << "Properties for " << it->first << std::endl;
        for (auto innerIt = it->second->cbegin(); !innerIt.is_end();
             innerIt++) {
            std::cout << "\t" << innerIt->first << " = " << innerIt->second
                      << std::endl;
        }
    }
}
