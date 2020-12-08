#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ncurses.h>
#include <fcntl.h>

#define STAR '*'
#define BUFSIZE 21

struct food{
    int x;
    int y;
};

struct direction{
    int x;
    int y;
};

struct snake{
    struct snake * next;
    int x;
    int y;
};

enum t_game_state{
    game_continue,
    game_over
};

void w_create_food(WINDOW * win, struct food * food, const int max_x, const int max_y)
{
    food->x = 2 + rand() % (max_x - 3);
    food->y = 2 + rand() % (max_y - 3);
    wmove(win, food->y, food->x);
    waddch(win, STAR);
}

void w_move_star(WINDOW * win, int x, int y)
{
    wmove(win, y, x);
    waddch(win, STAR);
}

void w_hide_star(WINDOW * win, struct snake * snake)
{
    while(snake->next)
        snake = snake->next;
    wmove(win, snake->y, snake->x);
    waddstr(win, "  ");
    wmove(win, snake->y + 1, snake->x);
    waddstr(win, "  ");
    wmove(win, snake->y - 1, snake->x);
    waddstr(win, "  ");
}

void w_move_snake(WINDOW * win, struct snake * snake, int offset_x, int offset_y)
{
    if(snake == NULL)
        return;
    w_move_snake(win, snake->next, snake->x, snake->y);
    snake->x = offset_x;
    snake->y = offset_y;
    wmove(win, snake->y, snake->x);
    waddch(win, STAR);
}

void w_check_border(WINDOW * win, struct snake * snake, const int max_x, const int max_y)
{
    if(snake->x >= max_x)
        snake->x = 1;
    else if(snake->x <= 0)
        snake->x = max_x - 1;
    if(snake->y >= max_y)
        snake->y = 1;
    else if(snake->y <= 0)
        snake->y = max_y - 1;
}

void w_check_food(WINDOW * win, struct snake * snake, struct food * food, int * points_per_game, const int max_x, const int max_y)
{
    if(snake->x == food->x && snake->y == food->y){
        ++*points_per_game;
        w_create_food(win, food, max_x, max_y);
        struct snake * tmp = malloc(sizeof(struct snake));
        tmp->next = NULL;
        while(snake->next)
            snake = snake->next;
        snake->next = tmp;        
        return;
    }
    wmove(win, food->y, food->x);
    waddch(win, STAR);
}

void check_state(struct snake * snake, enum t_game_state * game_state, int points_per_game)
{
    if(points_per_game == 0) return;
    int f_x = snake->x, f_y = snake->y;
    while(snake->next){
        snake = snake->next;
        if(snake->x == f_x && snake->y == f_y)
            *game_state = game_over;
    }
}

void w_func_egg(WINDOW * win, int max_x, int max_y, int * check_egg)
{
    wmove(win, max_y / 2, max_x / 2 - 10);
    waddstr(win, "It's for Nastya Sitnikova");
    wrefresh(win);
    sleep(5);
    wmove(win, max_y / 2 + 1, max_x / 2 - 10);
    waddstr(win, "You're beautiful <3");
    wrefresh(win);
    sleep(5);
    *check_egg = false;
}

void w_show_snake
    (WINDOW * win, struct snake * snake, struct food * food, const int max_x, const int max_y, int offset_x, int offset_y, int * points_per_game, enum t_game_state * game_state)
{
    w_hide_star(win, snake);
    box(win, '|', '-');
    w_move_snake(win, snake, snake->x + offset_x, snake->y + offset_y);
    check_state(snake, game_state, *points_per_game);
    w_check_food(win, snake, food, points_per_game, max_x, max_y);
    w_check_border(win, snake, max_x, max_y);
    box(win, '|', '-');
    wrefresh(win);
}

struct snake * give_memory(int count, int x, int y)
{
    struct snake * snake = NULL, * tmp, * first = NULL;
    int i;
    for(i = 0; i < count; ++i){
        tmp = malloc(sizeof(struct snake));
        tmp->next = NULL;
        if(!first){
            tmp->x = x;
            tmp->y = y;
            snake = tmp;
            first = tmp;
        } else {
            tmp->x = --x;
            tmp->y = y;
            snake->next = tmp;
            snake = snake->next;
        }
    }
    return first;
}

int str_cmp(const char * str1, const char * str2)
{
    int i;
    for(i = 0; ; ++i){
        if(str1[i] != str2[i])
            return 0;
        if(str1[i] == '\0' && str2[i] == '\0')
            break;
    }
    return 1;
}

/* networking functions */

void fill_send_buffer(int buf[], struct snake * snake, struct food * food, int points_per_game, enum t_game_state * game_state)
{
    buf[0] = snake->x;
    buf[1] = snake->y;
    buf[2] = food->x;
    buf[3] = food->y;
    buf[4] = points_per_game;
    buf[5] = *game_state;
}

void clean_window(WINDOW * win, int max_x, int max_y)
{
    int i, j;
    for(i = 0; i < max_y; ++i){
        for(j = 0; j < max_x; ++j){
            wmove(win, i, j);
            waddch(win, ' ');
        }
    }
}

void show_food(WINDOW * win, int buf[])
{
    wmove(win, buf[3], buf[2]);
    waddch(win, STAR);
}

void add_star(struct snake * snake)
{
    struct snake * tmp = malloc(sizeof(struct snake));
    tmp->next = NULL;
    while(snake->next)
        snake = snake->next;
    snake->next = tmp;
}

void show_another_snake(WINDOW * win, int buf[], struct snake * snake, int * previous_points_per_game, int * another_game_state)
{
    w_hide_star(win, snake);
    w_move_snake(win, snake, buf[0], buf[1]);
    show_food(win, buf);
    if(*previous_points_per_game < buf[4]){
        add_star(snake);
        *previous_points_per_game = buf[4];
    }
    if(buf[5] == game_over)
        *another_game_state = game_over;
    box(win, '|', '-');
    wrefresh(win);
}

int send_all(int sock, int * buffer, int sizebuf, int flags)
{
    int total = 0, temp;
    while(total < sizebuf){
        total += temp = send(sock, buffer + total, sizebuf - total, flags);
        if(temp == -1) break;
    }
    return (temp == -1 ? -1 : total);
}

int recv_all(int sock, int * buffer, int sizebuf, int flags)
{
    int total = 0, temp;
    while(total < sizebuf){
        total += temp = recv(sock, buffer + total, sizebuf - total, flags);
        if(temp == -1) break;
    }
    return (temp == -1 ? -1 : total);
}

/* --------------------- */

int main(int argc, char ** argv)
{
    srand(time(0));
    
    /* networking */
    int sock;
    struct sockaddr_in addr;
    int send_buffer[BUFSIZE], recv_buffer[BUFSIZE];
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425);
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("connect");
        exit(2);
    }
    
    unsigned int flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    /* ---------- */
    /* local player */
    
    
    /* ------------ */
    
    WINDOW * first_win, * second_win;
    
    int key,
        std_rows_y, std_cols_x,
        first_rows_y, first_cols_x,
        second_rows_y, second_cols_x,
        points_per_game = 0;
        
    /* another players */
    
    int previous_points_per_game = 0;
    int another_game_state = 0;
    
    /* --------------- */
    
    struct snake * first_snake = NULL;
    struct snake * second_snake = NULL;
    struct food * second_food = malloc(sizeof(struct food));
    struct direction * direction = malloc(sizeof(struct direction));
    enum t_game_state game_state = game_continue;
    
    initscr();
    cbreak();
    timeout(50);
    keypad(stdscr, 1);
    noecho();
    curs_set(0);
    
    getmaxyx(stdscr, std_rows_y, std_cols_x);
    
    /* windows alignment */
    
    send_buffer[0] = std_rows_y;
    send_buffer[1] = std_cols_x;
    send(sock, send_buffer, BUFSIZE, 0);
    sleep(2);
    recv(sock, recv_buffer, BUFSIZE, 0);    
    
    first_win = newwin(recv_buffer[0] - 1, recv_buffer[1] / 2 - 2, 1, 1);
    second_win = newwin(recv_buffer[0] - 1, recv_buffer[1] / 2 - 2, 1, std_cols_x - std_cols_x / 2);

    /* ----------------- */
    
    box(first_win, '|', '-');
    box(second_win, '|', '-');
    getmaxyx(first_win, first_rows_y, first_cols_x);
    getmaxyx(second_win, second_rows_y, second_cols_x);
    
    first_snake = give_memory(1, first_cols_x / 2 - 1, first_rows_y / 2);
    second_snake = give_memory(1, second_cols_x / 2 - 1, second_rows_y / 2);
    
    wmove(second_win, second_snake->y, second_snake->x);
    waddch(second_win, STAR);
    w_create_food(second_win, second_food, second_cols_x - 1, second_rows_y - 1);
    wrefresh(first_win);
    wrefresh(second_win);
    sleep(3);
    int counter = 0;
    while(1){
        key = getch();
        switch(key){
            case KEY_UP:
                if(direction->x == 0 && direction->y == 1) break;
                w_show_snake(second_win, second_snake, second_food, second_cols_x - 1, second_rows_y - 1, direction->x = 0, direction->y = -1, &points_per_game, &game_state);
                break;
            case KEY_DOWN:
                if(direction->x == 0 && direction->y == -1) break;
                w_show_snake(second_win, second_snake, second_food, second_cols_x - 1, second_rows_y - 1, direction->x = 0, direction->y = 1, &points_per_game, &game_state);
                break;
            case KEY_LEFT:
                if(direction->x == 1 && direction->y == 0) break;
                w_show_snake(second_win, second_snake, second_food, second_cols_x - 1, second_rows_y - 1, direction->x = -1, direction->y = 0, &points_per_game, &game_state);
                break;
            case KEY_RIGHT:
                if(direction->x == -1 && direction->y == 0) break;
                w_show_snake(second_win, second_snake, second_food, second_cols_x - 1, second_rows_y - 1, direction->x = 1, direction->y = 0, &points_per_game, &game_state);
                break;
                
            case ERR:
                w_show_snake(second_win, second_snake, second_food, second_cols_x - 1, second_rows_y - 1, direction->x, direction->y, &points_per_game, &game_state);
                break;
        }
        
        if(counter++ == 20){
            clean_window(first_win, std_cols_x / 2 - 2, std_rows_y - 1);
            continue;
        }
        fill_send_buffer(send_buffer, second_snake, second_food, points_per_game, &game_state);
        send_all(sock, send_buffer, BUFSIZE, 0);
        recv_all(sock, recv_buffer, BUFSIZE, 0);
        show_another_snake(first_win, recv_buffer, first_snake, &previous_points_per_game, &another_game_state);
        if(game_state == game_over || another_game_state == game_over) break;
    }
    char buf[2], another_buf[2];
    sprintf(buf, "%d", points_per_game);
    sprintf(another_buf, "%d", previous_points_per_game);
    wmove(second_win, second_rows_y / 2, second_cols_x / 2 - 5);
    waddstr(second_win, "GAME OVER");
    wrefresh(second_win);
    sleep(3);
    wmove(second_win, second_rows_y / 2, second_cols_x / 2 - 5);
    waddstr(first_win, "         ");
    wmove(second_win, second_rows_y / 2, second_cols_x / 2 - 10);
    waddstr(second_win, "Number of points: ");
    waddstr(second_win, buf);
    wmove(first_win, first_rows_y / 2, second_cols_x / 2 - 10);
    waddstr(first_win, "Number of points: ");
    waddstr(first_win, another_buf);
    wrefresh(first_win);
    wrefresh(second_win);
    sleep(2);
    endwin();
    return 0;
}
