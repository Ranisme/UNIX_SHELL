#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80 /* The maximum length command */

int check_input, check_output;
int input_index, output_index;



void getInput(char *line, char **args) {
    int num = 0;
    char *token = strtok(line, " ");
    while (token != NULL) {
        args[num++] = token;
        token = strtok(NULL, " ");
    }
    args[num] = NULL;

     //In ra để kiểm tra
    //  for (int i = 0; args[i] != NULL; i++) {
    //      printf("args[%d] = %s\n", i, args[i]);
    //  }
}

int check_Redirect(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (!strcmp(args[i],">")) {
            if(args[i + 1] == NULL) {
                printf("Please enter file\n");
                return 0;
            }
            else
                output_index = i + 1;
        }
        if (!strcmp(args[i], "<")) {
            if(args[i + 1] == NULL) {
                printf("Please enter file\n");
                return 0;
            }
            else
                input_index = i + 1;
        }
    }
    return 1;
}

int file_redirect(char **args) {
    /*******************************************************************
                        Kiểm tra và mở file input/output
    ********************************************************************/

    input_index = -1; output_index = -1;
    int ok = check_Redirect(args);

    if (ok == 0)
        return 0;

    // Nếu có file input
    if (input_index != -1) {
        int input_open = open(args[input_index], O_RDONLY);
        if (input_open == -1) { // không thể mở file
            printf("Can't open file\n");
            return 0;
        }
        dup2(input_open, STDOUT_FILENO);
        close(input_open);
        args[input_index] = NULL;
        args[input_index - 1] = NULL; // Xóa 2 phần tử cuối "< file"
    }

    if (output_index != -1) {
        int output_open = open(args[output_index], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        if (output_open == -1) { // không thể mở file
            printf("Can't open file\n");
            return 0;
        }
        dup2(output_open, STDOUT_FILENO);
        close(output_open);
        args[output_index] = NULL;
        args[output_index - 1] = NULL; // Xóa 2 phần tử cuối "> file"
    }

    return 1;
}

// char *args[MAX_LINE/2 + 1];

// void getPid(char *args[MAX_LINE/2 + 1]) {
//     for (int i = 0; i)
// }

int main(void)
{
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
    char *last_command[30]; // lệnh cuối cùng;
    int should_run = 1; /* flag to determine when to exit program */
    int not_first = 0;
    while (should_run) {
        printf("osh>");
        fflush(stdout);

        // Nếu không có lệnh thì tiếp tục chờ nhập lệnh khác
        char line[MAX_LINE];
        char pre_last[MAX_LINE];
        if (fgets(line, MAX_LINE, stdin) == NULL || line[0] == '\n') {
            continue;
        }

        // Loại bỏ kí tự xuống hàng ở cuối
        line[strcspn(line, "\n")] = 0;

        // Lưu lại line
        strcpy(pre_last, line);

        // Truyền Input vào
        getInput(line, args);

        // lệnh thoát
        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        /* ****************************************************************
                                    LỆNH LỊCH SỬ
        ******************************************************************* */
        if (strcmp(args[0], "!!") == 0) {
            if (not_first == 0) {
                printf("No commands in history.\n");
                continue;
            }
            else {
                printf("%s\n", last_command);
                char cur[MAX_LINE];
                strcpy(cur, last_command);
                getInput(cur, args);
            }
        }
        else { // nếu là lệnh mới thì lưu lại thành lệnh cuối cùng
            strcpy(last_command, pre_last);
            not_first = 1;
        }


        /**************************************************
                    Trường hợp có Pipe
        ***************************************************/

        char *pipe_cmd[2][MAX_LINE/2 + 1];
        int pipe_check = 0, er = 0, num0 = 0, num1 = 0;

        for (int i = 0; args[i] != NULL; i++) {
            if (!strcmp(args[i],"|")) {
                if(args[i + 1] == NULL) {
                    printf("Please enter second commad\n");
                    er = 1;
                }
                else {
                    pipe_check = 1;
                    continue;
                }
            }
            if (pipe_check == 0)
                pipe_cmd[0][num0++] = args[i];
            else
                pipe_cmd[1][num1++] = args[i];
        }

        if (er)
            continue;

        pipe_cmd[0][num0] = NULL;
        pipe_cmd[1][num1] = NULL;

        if (pipe_check) {
            int pipe_fd[2];
            pipe(pipe_fd);
            pid_t pid1 = fork();
            if (pid1 == 0) {
                // Tiến trình đầu tiên: Redirect stdout sang pipe
                close(pipe_fd[0]);  // Đóng đầu đọc của pipe
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                execvp(pipe_cmd[0][0], pipe_cmd[0]);
                printf("execvp failed\n");
                exit(1);
            } else {
                pid_t pid2 = fork();
                if (pid2 == 0) {
                    // Tiến trình thứ hai: Đọc từ pipe và thực thi
                    close(pipe_fd[1]);  // Đóng đầu ghi của pipe
                    dup2(pipe_fd[0], STDIN_FILENO);
                    close(pipe_fd[0]);
                    execvp(pipe_cmd[1][0], pipe_cmd[1]);
                    printf("execvp failed\n");
                    exit(1);
                }
            }
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            wait(NULL);
            wait(NULL);
            continue;
        }

        /**************************************************
                    Tạo tiến trình con bằng fork
        ***************************************************/
        pid_t pid = fork();
        if (pid == 0) { // Tiến trình con
            int ok = file_redirect(args);
            if (ok == 0)
                continue;
            execvp(args[0], args);
            printf("execvp failed");
            exit(0);
        }
        else if (pid > 0) {
            wait(NULL);
        }
        else {
            printf("error");
        }
    }
    return 0;
}
