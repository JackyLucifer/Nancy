#pragma once
#include <cassert>
#include <string.h>

namespace nc::http {

class parser {
    char* get_tar_next(char* str , const char *target); 
    HTTP_CODE parse_method(char *str);

public:
    LINE_STATUS parse_line(char* buf, int& parser_idx, int& read_idx );
    HTTP_CODE start_parse(int fd); 
    HTTP_CODE parse_requestline(int fd, char* line);
    HTTP_CODE parse_headers(int fd, char* line);
    HTTP_CODE parse_content(int fd, char* line, int parser_idx, int read_idx);
};
enum CHECK_STATE { 
    CHECK_STATE_REQUESTLINE = 0, 
    CHECK_STATE_HEADER, 
    CHECK_STATE_CONTENT 
};

enum LINE_STATUS {
    LINE_OK = 0, 
    LINE_BAD, 
    LINE_KPON 
};

enum HTTP_CODE {
    REQUEST_KPON, 
    REQUEST_GET, 
    REQUEST_POST,
    REQUEST_BAD, 
    REQUEST_END,             //读到尽头
    REQUEST_NET_ERROR,       //internet error  
    REQUEST_CLOSED_CONN      //closed connection
};



char* Parser::get_tar_next(char* str ,const char *target) {
    //找到包含在target中的目标字符，并返回其下一个字符，没有找到返回NULL
    char *ret = strpbrk(str, target);
    if (ret) {
        *ret++ = '\0';
        return ret;
    }
    else {
        return NULL;
    }
}

HTTP_CODE Parser::parse_method(char *str) {
    if (!strncasecmp(str, "GET" ,3)) {
        return  REQUEST_GET;
    }
    else if (!strncasecmp(str, "POST", 4)) {
        return REQUEST_POST;
    }
    return REQUEST_BAD;
}

HTTP_CODE Parser::start_parse(int fd) {
    //解析http报文的总函数，也是主从状态机器中的主状态机。
    //返回REQUEST_END->结束
    //返回REQUEST_KPON->继续读取
    char* buf = user[fd].read_buf;
    int& read_idx = user[fd].read_idx;
    int& parser_idx = user[fd].parser_idx;
    int& line_start_idx = user[fd].start_line_idx;
    char *buf_ptr;
    CHECK_STATE& check_state = user[fd].check_state;
    HTTP_CODE& method = user[fd].method;
    LINE_STATUS linestatus;
    HTTP_CODE retcode;
    while(check_state == CHECK_STATE_CONTENT && linestatus == LINE_OK || (linestatus = parse_line(buf, parser_idx, read_idx)) == LINE_OK  )
    {
        buf_ptr = buf + line_start_idx;
        line_start_idx = parser_idx;
        switch (check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                retcode = parse_requestline(fd, buf_ptr);
                if ( retcode == REQUEST_KPON ) {
                    check_state = CHECK_STATE_HEADER;
                }  
                else {
                    return REQUEST_BAD;
                }
                break;
            }
            case CHECK_STATE_HEADER: 
            {
                retcode = parse_headers(fd, buf_ptr);
                if ( retcode == REQUEST_GET ) {
                    method = REQUEST_GET;  //修改method
                    return REQUEST_GET; 
                }
                if ( retcode == REQUEST_POST ) {
                    method = REQUEST_POST; //修改method
                    check_state = CHECK_STATE_CONTENT;
                }
                if ( retcode == REQUEST_BAD ) {
                    return REQUEST_BAD;
                }
                break;
            }
            case CHECK_STATE_CONTENT: 
            {
                retcode = parse_content(fd, buf_ptr, parser_idx, read_idx);   
                if (retcode == REQUEST_POST) {
                    return REQUEST_POST;
                }
                break;
            }
            default: {
                return REQUEST_NET_ERROR;
            }
        }
    }
    if( linestatus == LINE_KPON ){
        return REQUEST_KPON;
    }
    else {
        return REQUEST_BAD;
    }
}

LINE_STATUS Parser::parse_line(char* buf, int& parser_idx, int& read_idx ) {
    //解析行，在buf中利用parser_idx指针定位到行尾，并将行尾的换行符号替换成'\0'
    char temp;
    for ( ; parser_idx < read_idx ; ++parser_idx )
    {
        temp = buf[parser_idx];
        if ( temp == '\r' ) 
        {
            if ( ( parser_idx + 1 ) == read_idx ) {
                return LINE_KPON;
            }
            else if ( buf[ parser_idx + 1 ] == '\n' )
            {
                buf[ parser_idx++ ] = '\0';
                buf[ parser_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if( temp == '\n' )
        {
            if( ( parser_idx > 1 ) &&  buf[ parser_idx - 1 ] == '\r' )
            {
                buf[ parser_idx-1 ] = '\0';
                buf[ parser_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_KPON;
}

HTTP_CODE Parser::parse_requestline(int fd, char* line )  {    
    //解析请求行(首行)，并更新用户信息表user。
    LOG("recv","%s", line);
    char* str_ptr = line;
    char* keep_url;
    HTTP_CODE method;
    str_ptr += strspn(str_ptr ," \t");
    //解析请求类型
    method = parse_method(str_ptr);
    if (method == REQUEST_BAD) {
        return REQUEST_BAD; 
    }
    //获取指向url的指针并赋值给:keep_url 
    str_ptr = get_tar_next(str_ptr, " \t");
    str_ptr += strspn(str_ptr, " \t");
    if (!str_ptr) {
        return REQUEST_BAD;
    }
    keep_url = str_ptr;
    
    //检验协议类型
    str_ptr = get_tar_next(str_ptr, " \t");
    str_ptr += strspn( str_ptr, " \t" );
    if (!strcasecmp( str_ptr, "HTTP/1.1"))  
        user[fd].protocol = "HTTP/1.1";
    else if (!strcasecmp( str_ptr, "HTTP/1.0"))
        user[fd].protocol = "HTTP/1.0";
    else 
        return REQUEST_BAD;

    //保存请求的url  
    if (!strncasecmp(keep_url, "http://", 7 )) 
    {
        keep_url += 7;
    }
    if (!strncasecmp(keep_url, "https://", 8 )) 
    {
        keep_url += 8;
    }
    keep_url = strchr( keep_url, '/' );    
    if (!keep_url || keep_url[0] != '/') 
    {
        return REQUEST_BAD;
    }
    else 
    {
        user[fd].url = keep_url+1;
    } 
    return REQUEST_KPON;
}

HTTP_CODE Parser::parse_headers(int fd, char* line) {
    //解析请求头信息
    char* str_ptr = line;
    if (!strncasecmp( str_ptr, "Host:", 5 ))
    {
        LOG("recv","%s", str_ptr);
        str_ptr += 5;
        str_ptr += strspn( str_ptr, " \t" );
        //printf( "the request host is: %s\n", str_ptr );
    }
    else if (!strncasecmp( str_ptr, "Connection:", 11))
    {
        LOG("recv","%s", str_ptr);
        str_ptr += 11;
        str_ptr += strspn(str_ptr, " \t");
        if (strcasecmp(str_ptr, "keep-alive") == 0) 
            user[fd].keep_alive = true; 
        else
            user[fd].keep_alive = false; 
        
    }
    else if (strncasecmp(str_ptr, "Content-length:", 15) == 0)
    {
        LOG("recv","%s", str_ptr);
        str_ptr += 15;
        str_ptr += strspn(str_ptr, " \t");
        user[fd].content_len = atol(str_ptr);
    }
    else if (str_ptr[0] == '\0') {
        //读到末尾，以是否附加内容确定方法
        if (user[fd].content_len > 0) {
            return REQUEST_POST;
        }
        return REQUEST_GET;
    }
    return REQUEST_KPON;
}

HTTP_CODE Parser::parse_content(int fd, char* line, int parser_idx, int read_idx) {
    //解析post附带内容
    if (read_idx < parser_idx + user[fd].content_len) {
        return REQUEST_KPON;
    }
    LOG("recv","content:%s",line);
    user[fd].content = line; 
    user[fd].content[user[fd].content_len] = '\0';
    return REQUEST_POST;
}


} // namespace nc::http