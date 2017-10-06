/* We demonstrate how to nest hash tables within one another, to store
 * unstructured data, kind of like JSON. There's still the limitation that it's
 * statically typed. */

/* MSVC genenrates warning C4503 if the decorated name is longer than the 
 * compiler limit (4096).
 * In this exmaple, C4503 is not a concern. We can simply suppress it here.
 * for details: 
 * https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-1-c4503
*/
#ifdef _MSC_VER
#pragma warning(disable:4503)
#endif

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>

typedef cuckoohash_map<std::string, std::string> InnerTable;
typedef cuckoohash_map<std::string, std::unique_ptr<InnerTable>> OuterTable;

int main() {
  OuterTable tbl;

  tbl.insert("bob", std::unique_ptr<InnerTable>(new InnerTable));
  tbl.update_fn("bob", [](std::unique_ptr<InnerTable> &innerTbl) {
    innerTbl->insert("nickname", "jimmy");
    innerTbl->insert("pet", "dog");
    innerTbl->insert("food", "bagels");
  });

  tbl.insert("jack", std::unique_ptr<InnerTable>(new InnerTable));
  tbl.update_fn("jack", [](std::unique_ptr<InnerTable> &innerTbl) {
    innerTbl->insert("friend", "bob");
    innerTbl->insert("activity", "sleeping");
    innerTbl->insert("language", "javascript");
  });

  {
    auto lt = tbl.lock_table();
    for (const auto &item : lt) {
      std::cout << "Properties for " << item.first << std::endl;
      auto innerLt = item.second->lock_table();
      for (auto innerItem : innerLt) {
        std::cout << "\t" << innerItem.first << " = " << innerItem.second
                  << std::endl;
      }
    }
  }
}
