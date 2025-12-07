#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char* getcgidata(FILE* fp, char* requestmethod);
int main()
{
        char *input;
        char *req_method;
        char name[64];
        char pass[64];
        int i = 0;
        int j = 0;

		// printf("Content-type: text/plain; charset=iso-8859-1\n\n");
        printf("Content-type: text/html\n\n");
        printf("The following is query reuslt:<br><br>");
        req_method = getenv("REQUEST_METHOD");
		
		//它是從 環境變數中取得 HTTP 請求的方法（Method），用來判斷使用者送來的是哪種 HTTP 方法。
        input = getcgidata(stdin, req_method);
		// input = username=yourname&password=yourpass
        for ( i = 9; i < (int)strlen(input); i++ )
        {
               if ( input[i] == '&' )
               {
                      name[j] = '\0';
                      break;
               }
               name[j++] = input[i];
        }
		
        for ( i = 19 + strlen(name), j = 0; i < (int)strlen(input); i++ )
        {
               pass[j++] = input[i];
        }
        pass[j] = '\0';
        printf("Your Username is %s<br>Your Password is %s<br> \n", name, pass);

        return 0;
}

char* getcgidata(FILE* fp, char* requestmethod)
{
        char* input;
        int len;
        int size = 1024;
        int i = 0;

        if (!strcmp(requestmethod, "GET"))
        {
               input = getenv("QUERY_STRING");
               return input;
        }
        else if (!strcmp(requestmethod, "POST"))
        {
               len = atoi(getenv("CONTENT_LENGTH"));
               input = (char*)malloc(sizeof(char)*(size + 1));

               if (len == 0)
               {
                      input[0] = '\0';
                      return input;
               }

               while(1)
               {
                      input[i] = (char)fgetc(fp);
                      if (i == size)
                      {
                             input[i+1] = '\0';
                             return input;
                      }

                      --len;
                      if (feof(fp) || (!(len)))
                      {
                             i++;
                             input[i] = '\0';
                             return input;
                      }
                      i++;
               }
        }
        return NULL;
}


