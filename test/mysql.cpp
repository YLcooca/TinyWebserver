#include <mysql/mysql.h>

#include <iostream>
#include <string>

using mysql = MYSQL;

int main() {
  mysql_server_init(0, nullptr, nullptr);
  auto sql = mysql_init(nullptr);
  if (!sql) {
    std::cout << "mysql init fail" << std::endl;
  }
  if (!mysql_real_connect(sql, "localhost", "root", "12345678", "webserver", 3306,
                          NULL, 0)) {
    std::cout << "mysql connection error!" << std::endl;
  }
  std::string query =
      "SELECT username,password FROM user WHERE username='root'";
  if (mysql_query(sql, query.data())) {
    std::cout << "query error!" << std::endl;
  }
  auto res = mysql_store_result(sql);
  if (auto row = mysql_fetch_row(res)) {
    std::cout << row[0] << ", " << row[1] << std::endl;
  }
  mysql_free_result(res);
  mysql_close(sql);
  mysql_server_end();
  return 0;
}