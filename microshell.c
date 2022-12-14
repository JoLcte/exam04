#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct s_list List;

enum	_type
{
	CMD,
	PIPE,
	CMD_END
};

struct s_list
{
	List	*next;
	char	**data;
	int	type;
};

List	*g_list;

int	ft_strlen(char const *s)
{
	char const *save = s;
	while (*s)
		++s;
	return (s - save);
}

void	clear_list(List *list)
{
	List *tmp = list;

	while (list)
	{
		list = list->next;
		free(tmp);
		tmp = list;
	}
}

void	exit_fatal(void)
{
	write(2, "error: fatal\n", 13);
	clear_list(g_list);
	exit(1);
}

void	exit_cmd_error(char const *cmd)
{
	write(2, "error: cannot execute ", 22);
	write(2, cmd, ft_strlen(cmd));
	write(2, "\n", 1);
	clear_list(g_list);
	exit(127);
}

void	push_list(List **list, char **av, int size, int type)
{
	if ( !( (*list)->next = malloc(sizeof(List)) ) )
		exit_fatal();
	*list = (*list)->next;
	(*list)->data = av - size;
	(*list)->type = type;
	(*list)->next = NULL;
	if (type)
		*av = NULL;
}

void	parser(List *list, char **av, int ac)
{
	int i = 1;
	int j = 1;

	while (i < ac)
	{
		if (av[i][0] == '|' && !av[i][1])
		{
			push_list(&list, av + i, i - j, PIPE);
			j = i + 1;
		}
		else if (av[i][0] == ';' && !av[i][1])
		{
			push_list(&list, av + i, i - j, CMD_END);
			j = i + 1;
		}	
		++i;
	}
	push_list(&list, av + j, 0, CMD);
}

int	_cd(List *list)
{
	if (!list->data[1] || list->data[2])
	{
		write(2, "error: cd: bad arguments\n", 25);
		return 1;
	}
	if (chdir(list->data[1]))
	{
		write(2, "error: cd: cannont change directory to ", 38);
		write(2, list->data[1], ft_strlen(list->data[1]));
		write(2, "\n", 1);
		return 2;
	}
	return 0;
}

int	simple_exec(List **list, char **envp)
{
	int pid = 0, status = 0;

	if (!(*list)->data[0])
	{
		*list = (*list)->next;
		return status;
	}
	if (!(strcmp((*list)->data[0], "cd")))
	{
		status = _cd(*list);
		*list = (*list)->next;
		return status;
	}
	pid = fork();
	if (pid < 0)
		exit_fatal();
	if (pid == 0)
	{
		execve((*list)->data[0], (*list)->data, envp);
		exit_cmd_error((*list)->data[0]);
	}
	waitpid(pid, &status, WUNTRACED);
	*list = (*list)->next;
	return status;
}

void	start_pipe(List *list, char **envp, int *fds)
{
	int pid = 0;

	if (pipe(fds) < 0)
		exit_fatal();
	pid = fork();
	if (pid < 0)
		exit_fatal();
	if (pid == 0)
	{
		dup2(fds[1], 1);
		close(fds[0]);
		close(fds[1]);
		execve(list->data[0], list->data, envp);
		exit_cmd_error(list->data[0]);
	}
	close(fds[1]);
}

void	mid_pipe(List *list, char **envp, int *fds)
{
	int pid = 0, old_in = fds[0];

	if (pipe(fds) < 0)
		exit_fatal();
	pid = fork();
	if (pid < 0)
		exit_fatal();
	if (pid == 0)
	{
		dup2(old_in, 0);
		dup2(fds[1], 1);
		close(fds[0]);
		close(old_in);
		close(fds[1]);
		execve(list->data[0], list->data, envp);
		exit_cmd_error(list->data[0]);
	}
	close(old_in);
	close(fds[1]);
}

int	end_pipe(List *list, char **envp, int *fds)
{
	int pid = 0;

	pid = fork();
	if (pid < 0)
		exit_fatal();
	if (pid == 0)
	{
		dup2(fds[0], 0);
		close(fds[0]);
		close(fds[1]);
		execve(list->data[0], list->data, envp);
		exit_cmd_error(list->data[0]);
	}
	close(fds[0]);
	return (pid);
}

int	pipe_exec(List **list, char **envp)
{
	int ret = 0, pid = 0, status = 0, nb_pid = 0, fds[2];

	start_pipe(*list, envp, fds);
	++nb_pid;
	*list = (*list)->next;
	while ((*list)->type == PIPE)
	{
		mid_pipe(*list, envp, fds);
		++nb_pid;
		*list = (*list)->next;
	}
	pid = end_pipe(*list, envp, fds);
	++nb_pid;
	while (nb_pid)
	{
		if (waitpid(-1, &status, WUNTRACED) == pid)
			ret = status;
		--nb_pid;
	}
	*list = (*list)->next;
	close(fds[1]);
	return ret;
}

int	exec_cmd(List *list, char **envp)
{
	int ret;

	while (list)
	{
		if (list->type == PIPE)
			ret = pipe_exec(&list, envp);
		else
			ret = simple_exec(&list, envp);
	}
	clear_list(g_list);
	return ret;
}

int	main(int ac, char **av, char **envp)
{
	List list;

	if (ac == 1)
		return 0;
	parser(&list, av, ac);
	g_list = list.next;
	return (exec_cmd(list.next, envp));
}
