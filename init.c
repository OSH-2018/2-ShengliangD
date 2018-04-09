#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#include <stdlib.h>
#include <assert.h>

#define ASSERT()

#define ARG_NUM_MAX 256
#define JOB_NUM_MAX 256
typedef struct {
    char *args[ARG_NUM_MAX];

    int stdin;
    int stdout;
    int stderr;    
} job_cmd_t;
job_cmd_t job_cmds[JOB_NUM_MAX];

char *skip_space(char **ptr) {
    while (**ptr == ' ')
        ++*ptr;
    return *ptr;
}

// 复制到回车或 stop
char *get_string_until(char stop, char **ptr) {
    int cnt = 0;
    while ((*ptr)[cnt] != stop && (*ptr)[cnt] != '\n' && (*ptr)[cnt] != '\0')
        ++cnt;

    // 申请内存，复制
    char *ret = (char *)malloc(sizeof(char)*(cnt+1));
    for (int i = 0; i < cnt; ++i)
        ret[i] = (*ptr)[i];
    *ptr += cnt;

    return ret;
}

job_cmd_t parse_job_cmd(char **seek) {
    job_cmd_t jcmd;
    jcmd.stdin = jcmd.stdout = jcmd.stderr = -1;

    int cnt = 0;
    skip_space(seek);
    while (**seek != '\0' && **seek != '|') {
        char *fname = NULL;
        int append = 0;
        switch (**seek) {
            case '\'':  // 到下一个 '\'' 为止
                ++*seek;
                jcmd.args[cnt++] = get_string_until('\'', seek);
                ++*seek;
                break;
            case '\"':  // 到下一个 '"' 为止
                ++*seek;
                jcmd.args[cnt++] = get_string_until('\"', seek);
                ++*seek;
                break;
            case '>':  // 检查下一个字符是 '>' 还是其他
                ++*seek;
                switch (**seek) {
                    case '>':  // 追加
                        ++*seek;
                        append = 1;
                        break;
                    default:
                        append = 0;
                }
                skip_space(seek);
                fname = get_string_until(' ', seek);
                jcmd.stdout = open(
                        fname,
                        O_CREAT | O_WRONLY | (append? O_APPEND : 0),
                        S_IRUSR | S_IWUSR | S_IRGRP
                    );
                free(fname);
                if (jcmd.stdout == -1) {
                    perror("STDIN");
                }
                if (append)
                    lseek(jcmd.stdout, 0, SEEK_END);
                break;
            case '<':
                ++*seek;
                skip_space(seek);
                fname = get_string_until(' ', seek);
                jcmd.stdin = open(fname, O_RDONLY);
                free(fname);
                if (jcmd.stdin == -1) {
                    perror("STDOUT");
                }
                break;
            default:  // 当做普通连续字符串对待
                jcmd.args[cnt++] = get_string_until(' ', seek);
        }
        skip_space(seek);
    }
    jcmd.args[cnt] = NULL;
    return jcmd;
}

void parse_line(char **seek) {
    int cnt = 0;
    while (1) {
        job_cmds[cnt++] = parse_job_cmd(seek);
        skip_space(seek);
        if (**seek != '|')
            break;
        ++*seek;
    }
    job_cmds[cnt].args[0] = NULL;
}

void run_jobs() {
    job_cmd_t *jcmd = job_cmds;

    // 对于每个进程，把父进程对前一个进程的管道读端作为 stdin
    int prev_pipe_read = -1;
    while (jcmd->args[0] != NULL) {
        if (strcmp(jcmd->args[0], "cd") == 0) {
            if (jcmd->args[1])
                chdir(jcmd->args[1]);
            continue;
        }
        if (strcmp(jcmd->args[0], "pwd") == 0) {  // TODO
            char wd[4096];
            puts(getcwd(wd, 4096));
            continue;
        }
        if (strcmp(jcmd->args[0], "exit") == 0)  // TODO
            return;
        if (strcmp(jcmd->args[0], "export") == 0)  // TODO
            continue;

        // 对于每个 job，该进程记录读段，job 用写端代替 stdout
        int pipefd[2];
        assert( pipe(pipefd) == 0 );

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);   // job 不读

            if (prev_pipe_read != -1)
                assert( dup2(prev_pipe_read, STDIN_FILENO) != -1 );

            if ((jcmd+1)->args[0] != NULL)  // 如果不是被管道连接的最后一节，则用管道写端作为标准输出
                assert( dup2(pipefd[1], STDOUT_FILENO) != -1);

            if (jcmd->stdin != -1)
                assert( dup2(jcmd->stdin, STDIN_FILENO) != -1);
            if (jcmd->stdout != -1)
                assert( dup2(jcmd->stdout, STDOUT_FILENO) != -1);
            if (jcmd->stderr != -1)
                assert( dup2(jcmd->stderr, STDERR_FILENO) != -1);

            execvp(jcmd->args[0], jcmd->args);

            perror(jcmd->args[0]);
            exit(255);
        }
        close(pipefd[1]);
        close(prev_pipe_read);
        prev_pipe_read = pipefd[0];

        ++jcmd;
    }
}

int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    while (1) {
        /* 提示符 */
        printf("# ");
        fflush(stdin);
        if (fgets(cmd, 256, stdin) == NULL)
            break;

        /* 清理结尾的换行符 */
        {
            int i;
            for (i = 0; cmd[i] != '\n'; i++)
                ;
            cmd[i] = '\0';
        }

        /* 拆解命令行
            以管道为单位切分，填充 job 数据结构，然后逐一执行
         */
        {
            char *seek = cmd;
            parse_line(&seek);
        }

        run_jobs();
        
        // 等待所有子进程结束
        while (wait(NULL) > 0)
            ;
    }
}
