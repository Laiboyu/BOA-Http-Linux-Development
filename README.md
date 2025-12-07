# BOA-Http-Linux-Development
在此範例中會透過 Orangepi 來建立了一個 HTTP server。
其中 HTTP server 會以 BOA 來進行實作，會介紹 BOA 的觀念以及流程，並且會結合 CGI（Common Gateway Interface）來呼叫外部可執行檔來完成動態頁面功能，給使用者作為輸入資料使用。

## BOA的使用介紹
### BOA 是什麼?
Boa 是一個由 Larry Doolittle 於 1990 年代開發的 輕量級 HTTP 伺服器 (Lightweight HTTP server)。
它的主要目標是提供「小巧、快速、低記憶體佔用」的 Web Server，讓嵌入式裝置也能運行簡易的 Web 介面（例如路由器設定頁）。

它只專注於處理 HTTP 請求，沒有多餘的功能，但支援 CGI (Common Gateway Interface) 來實現動態內容。

[boa官網連結](http://www.boa.org/)
![02](https://hackmd.io/_uploads/rJ9Fj2llbx.png)

#### 架構與運作流程

Boa 採用 單行程 (Single Process) 架構，所有 HTTP 請求由同一個主行程處理：

```
+---------------------------+
|         Boa 進程          |
+-----------+---------------+
            |
            |-- 解析 HTTP Request
            |
            |-- 若為靜態頁面 => 讀檔回傳
            |
            |-- 若為 CGI Request => fork 子行程執行 CGI
            |
            '-- 回傳 Response 給 Client
```

Boa 流程簡化圖：
1. Client 發出 GET /index.html
2. Boa 收到並解析請求
3. 檢查檔案路徑 /var/www/index.html
4. 讀取檔案內容
5. 回傳 HTTP Response

### CGI 是什麼？

CGI（Common Gateway Interface） 是一種 Web Server 與外部應用程式之間的通訊協定。

Boa 可以透過呼叫 CGI 來呼叫外部可執行檔來完成動態頁面。當使用者在瀏覽器請求一個 CGI 程式時，Web Server 會啟動該程式，讓它執行一些邏輯（例如讀感測器資料、讀取設定值），然後輸出結果（通常是 HTML）。

CGI 基本原理是，瀏覽器傳送請求(Request)給伺服器，伺服器以環境變數的方式將使用者傳送的資料傳給「外部可執行程式」，讓外部程式根據這些資訊來執行，並將結果傳回給伺服器，伺服器再將結果傳回給瀏覽器。也就是說，伺服器將工作打包給外部程式做，做完之後得到結果送回給客戶端，所以才叫做 Gateway，因為伺服器只負責指派任務。

若 /var/www/cgi-bin/status.cgi 存在且可執行，Boa 就會：
1. fork 子行程執行該 CGI 程式
2. 將環境變數 (HTTP headers, query string 等) 傳入程式
3. 程式輸出透過 stdout 回傳給瀏覽器

詳細結合範例如下所示

##  BOA 啟動詳細流程介紹
### 1. BOA 啟動階段

使用**sudo boa**啟動Boa後，首先會去打開 **/etc/boa/boa.conf**，並解析其中的設定內容。
這些設定會影響伺服器行為：
```
Port 80
User nobody
Group nogroup
DocumentRoot /var/www
DirectoryIndex index.html
ErrorLog /var/log/boa/error_log
AccessLog /var/log/boa/access_log
ScriptAlias /cgi-bin/ /var/www/cgi-bin/
```
Boa 會依序：
* 設定監聽的埠號（預設 80）
* 設定伺服器的檔案根目錄
* 指定首頁檔案名稱
* 開啟 log 系統
* 設定 CGI 的實際目錄映射 
    
### 2. 建立 Socket 
    
Boa 啟動後，進入主要的 HTTP 服務迴圈。
在這裡，它會建立一個 TCP server socket：
```
sockfd = socket(AF_INET, SOCK_STREAM, 0);
bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
listen(sockfd, SOMAXCONN);
```
* AF_INET：使用 IPv4 網路
SOCK_STREAM：TCP 流式通訊
* bind()：綁定 IP 與 Port（通常是 80）
* listen()：開始監聽連線請求

**此時 Boa 已經成為一個「等待 HTTP Client 的 TCP Server」。**

### 3. 等待瀏覽器連線

瀏覽器輸入：
```
http://192.168.0.1/
```
Browser 會：
1. 對伺服器建立 TCP 連線到 port 80
2. 發送 HTTP Request
    ```
    GET / HTTP/1.0
    Host: 192.168.0.1
    User-Agent: Chrome/130.0
    Accept: text/html
    ```
Boa 接收到連線後
```
client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
```
此時產生一個新的 socket (client_fd) 對應這個客戶端。
    
### 4. 解析 HTTP Request

Boa 接著從 client_fd 讀取資料：
```
read(client_fd, buffer, sizeof(buffer));
```
範例請求：
```
GET /index.html HTTP/1.0
Host: 192.168.0.1
User-Agent: Mozilla/5.0
```
解析流程：
1. 取得 HTTP 方法（GET / POST）
2. 解析請求 URL 路徑
3. 檢查是否為 CGI 路徑（/cgi-bin/）
4. 決定實際對應的檔案路徑：
    ```
    /var/www/index.html
    ```
5. 檢查檔案是否存在、是否可讀


### 5. 靜態檔案處理

如果請求不是 /cgi-bin/，會去進行靜態檔案處理（Static File Serving），流程如下:
1. Boa 開啟目標檔案
    ```
    fd = open("/var/www/index.html", O_RDONLY);
    ```
2. 取得檔案大小與類型
    * .html → text/html
    * .jpg → image/jpeg
    * .css → text/css
3. 傳送 HTTP 回應
    ```
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-Type: text/html\r\n");
    printf("Content-Length: 512\r\n\r\n");
    ```
4. 使用 sendfile() 或 write() 將檔案內容傳給瀏覽器
    ```
    sendfile(client_fd, fd, NULL, filesize);
    ```

### 6. CGI 程式處理

如果請求的 URL 為 **/cgi-bin/...**，會去進行CGI 程式處理（Dynamic Content via CGI），來執行動態介面功能，Boa 會進行流程如下:
1. fork() 一個子行程
2. 設定 CGI 環境變數（由 HTTP Header 與 Query String 轉換而來）
    * REQUEST_METHOD
    * QUERY_STRING
    * CONTENT_LENGTH
    * REMOTE_ADDR
    * SERVER_NAME
3. redirect stdout → client socket
    ```
    dup2(client_fd, STDOUT_FILENO);
    execve("/var/www/cgi-bin/status.cgi", argv, envp);
    ```
4. CGI 程式執行並輸出
    ```
    Content-type: text/html
    <html><body>System OK</body></html>
    ```
5. Boa 把這些輸出直接傳給 Browser。

### 7. 回傳 HTTP Response

最後會回傳 HTTP Response 給 Browser，Boa 回傳的資料格式如下：
```
HTTP/1.0 200 OK
Server: Boa/0.94.13
Content-Type: text/html
Content-Length: 1024

<html><body><h1>Hello from Boa!</h1></body></html>
```
Browser 收到後會：
1. 根據 Content-Type 解析內容
2. 根據 HTML/CSS/JS 呈現畫面
3. 若網頁中有其他資源（圖片、JS、CSS），再發送更多請求

### 8. 關閉連線

傳輸完成後：
* Boa 關閉客戶端 socket：
    ```
    close(client_fd);
    ```
* 若設定 Keep-Alive，則可重複使用同一連線
* 回到主迴圈繼續 accept() 新連線

## BOA 啟動使用範例
### 移植BOA步驟
1. 取得 Boa 原始碼
    Boa 官方專案已停止維護，但在嵌入式系統中還是大量使用的範例。
    可以從 github 中的各路大神專案中取得：
    
    本範例是使用開源的source去建立boa的執行檔，所使用的開源專案連結如下:
    ```
    https://github.com/timmattison/boa
    ```

2. 設定cross-compilation（交叉編譯設定）的 configure 腳本
    ```
    CC=aarch64-linux-gnu-gcc  AR=aarch64-linux-gnu-ar  ./configure --prefix=/home/joey/sqlite/exe --host=aarch64-linux-gnu
    ```

3. 使用 make 進行編譯，產出boa執行檔

    首先先使用 make command
    ```
    make
    ```
    成功後會產生：
    ```
    src/boa
    ```
    這就是你要移植到板子上的執行檔。

4. 建立 ServerRoot 目錄
![06](https://hackmd.io/_uploads/Bkm9ggcgZe.png)
    * www: 所使用的html檔存放位置
    * boa: 為所建立給Orangepi使用的Boa執行檔
    * boa.conf: Boa的相關設定檔
    * pass.cgi: 給Boa執行的CGI檔

5. 設定Boa相關設定，Boa的主要設定檔位於:
    ```
    /boa/boa.conf
    ```
    設定範例（最小設定）：

    ```
    # /etc/boa/boa.conf
    Port 80
    User nobody
    Group nogroup
    DocumentRoot /var/www
    DirectoryIndex index.html
    ErrorLog /var/log/boa/error_log
    AccessLog /var/log/boa/access_log
    ````
### BOA 程式所使用 CGI 範例 
接下來會去進行 CGI 程式的說明，在此範例中建立了一個能夠從環境變數中取得 HTTP 請求的方法(GET/POST)，去取得使用者資料的方式，

接著去解析 **username** 和 **password**參數，最後回傳 HTML 格式輸出，在顯示bowser使用者輸入的帳號以及密碼。

```javascript = 
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

    //從環境變數中取得HTTP 請求的方法（Method），用來判斷使用者送來的是哪種 HTTP 方法。
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
```

解析 **username** 和 **password** 說明:
* 從第 9 個字元開始 (i = 9)，正好跳過 "username="，遇到 & 就停止(表示 username 字串結束)，將讀到的字元存入 name 陣列，並加上結尾字元 \0。
* 19 + strlen(name) 是計算 password 開始的索引，將剩下的字元存入 pass 陣列，並加結尾字元 \0
    * "username=" → 9 個字元 + "&password=" → 10 個字元 = 19 個字元

**getcgidata()** function中，輸入 HTTP 方法比較 
1. GET 方法：
    直接從環境變數 QUERY_STRING 取得 URL 參數
    範例：username=Joey&password=1234
2. POST 方法：
    讀取環境變數 CONTENT_LENGTH → 表示資料長度
    透過 malloc 分配緩衝區存資料

```javascript = 

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
```
    
### 實際啟動BOA步驟
1. 啟動伺服器
    ```
    ./boa -c /home/orangepi/boa
    ```
    * -c 指定的是 **ServerRoot** 目錄，不是檔案。Boa 會自動讀取該目錄下的 boa.conf（配置檔）、www/（網頁文件根目錄）、cgi-bin/（CGI 腳本）。

    此時在這裡已經透過 Orangepi 建立了一個 boa 的 HTTP server。
    
2. 利用放一個簡單的 HTML 網頁到 **/www/index.html**
    * 使用 GET 傳輸，提交目標為 /cgi-bin/pass.cgi
    * Browser 會將資料組成 Query String：
        /cgi-bin/pass.cgi?Username=xxx&Password=yyy
    並送到 Boa。Boa 檢查到 /cgi-bin/ → 執行對應的 CGI 程式。
    ```
    <html>
    <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title>使用者驗證</title>
    </head>
    <body>
    <form name="form1" action="/cgi-bin/pass.cgi" method="GET">
    <table align="center">
         <tr><td align="center" colspan="2"></td></tr>
         <tr>
            <td align="right">用戶名</td>
            <td><input type="text" name="Username"></td>
         </tr>
         <tr>
            <td align="right">密 碼</td>
            <td><input type="password" name="Password"></td>
         </tr>
         <tr>
            <td><input type="submit" value="登 錄"></td>
            <td><input type="reset" value="取 消"></td>
         </tr>
    </table>
    </form>
    </body>
    </html>
    ```
3. 用瀏覽器打開，輸入所使用的 Orangepi 的 IP：
    ```
    http://localhost
    ```
    解析 Http 後，就會看到網頁出現。
    ![04](https://hackmd.io/_uploads/SkNipVDebg.png)
    
    接著輸入使用者資料後按下登入，Browser 會去送出 GET request，利用先前所建立的 CGI 程式來解析 Http request，並且透過 Boa 將處理成果輸出給 bowser。
    ![05](https://hackmd.io/_uploads/Sy4jTVDxWg.png)
    
    目前已經完成了一個使用 Orangepi 來進行 Boa 範例程式，可以做為簡易且實用的嵌入式系統 Web UI 架構！。

參考資料:
http://www.boa.org/
https://github.com/timmattison/boa
https://zhuanlan.zhihu.com/p/471033375
