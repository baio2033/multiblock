#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> 
#include <string.h>


static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   printf("callback\n");
   return 0;
}


int main(int argc, char* argv[]) {
   FILE *fp = fopen("top.csv","r");
   char buf[256] = {0x00, };
   sqlite3 *db;
   char *zErrMsg = 0;
   int rc;
   char *sql, *str, *add;
   char *insert;
   char *tmp;

   rc = sqlite3_open("sql_db.db", &db);

   if( rc ) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      return(0);
   } else {
      fprintf(stderr, "Opened database successfully\n");
   }
   sql = (char *)malloc(sizeof(char)*256);
   memset(sql, 0x00, sizeof(sql));
   sql = "CREATE TABLE block_url (url LONGVARCHAR NOT NULL);";
   printf("%s\n",sql);   

   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   if( rc != SQLITE_OK ){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } 
   else 
      fprintf(stdout, "Records created successfully\n");
      
   insert = (char*)malloc(256);
   memset(insert, 0x00, sizeof(insert));
   char *ret;
   

   while((ret = fgets(buf,sizeof(buf),fp))){
      if(ret == NULL) break;
      
      str = strstr(buf,",") + 1;
      
      strcpy(insert, "insert into block_url(url) values(\"");
      tmp = (char *)malloc(sizeof(char)*strlen(str));
      memset(tmp, 0x00, strlen(str));

      strncpy(tmp, str, strlen(str)-1);  
      strcat(insert, tmp);      
      add = "\");";     

      strncat(insert, add, strlen(add));
      printf("\t[+] %s\n", insert);
      
      rc = sqlite3_exec(db, insert, callback, 0, &zErrMsg);  

      if( rc != SQLITE_OK ){
         fprintf(stderr, "SQL error: %s\n", zErrMsg);
         sqlite3_free(zErrMsg);
      } 
      else 
         fprintf(stdout, "Records created successfully\n");
      
      free(tmp);                
   }

   free(insert);
   //free(sql);
   sqlite3_close(db);
   fclose(fp);

   printf("finish\n");
   return 0;
}
