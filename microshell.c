#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>

typedef struct s_list List;

enum _types
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

int	ft_strlen( char const *s )
{
	char const *save = s;
	while (*s)
		++s;
	return (s - save);
}

void	clear_list( List *list )
{
	List *tmp = list;
	while (list)
	{
		list = list->next;
		free(tmp);
		tmp = list;
	}
}

void	exit_fatal( void )
{
	write(2, "error: fatal\n", 13);
	clear_list(g_list);
	exit(1);
}

void	exit_cmd_error( char const *cmd )
{
	write(2, "error: cannot execute ", 22);
	write(2, cmd, ft_strlen(cmd));
	write(2, "\n", 1);
	clear_list(g_list);
	exit(127);
}

void	push_list( List **list, char **av, int size, int type )
{
	if ( !( (*list)->next = malloc(sizeof(List)) ) )
		exit_fatal();
	(*list) = (*list)->next;
	(*list)->data = av - size;
	(*list)->next = NULL;
	(*list)->type = type;
	if (type)
		*av = NULL; //for execve to stop
}

void	parser( List *list, char **av, int ac )
{
	int i = 1;
	int j = 1;

	while ( i < ac )
	{
		if ( av[i][0] == '|' && !av[i][1] )
		{
			push_list( &list, av + i, i - j, PIPE );
			j = i + 1;
		}
		else if ( av[i][0] == ';' && !av[i][1] )
		{
			push_list( &list, av + i, i - j, CMD_END );
			j = i + 1;
		}
		++i;
	}
	push_list( &list, av + j, 0, CMD );
}

int		_cd( List **list )
{
	int ret = 0;

	if ( !((*list)->data[1]) || (*list)->data[2] )
	{
		write(2, "error: cd: bad arguments\n", 25);
		ret = 2;
	}
	else if ( chdir((*list)->data[1]) )
	{
		write(2, "error: cd: cannot change directory to ", 38);
		write(2, (*list)->data[1], ft_strlen((*list)->data[1]));
		write(2, "\n", 1);
		ret = 2;
	}
	(*list) = (*list)->next;
	return ret;
}

int		simple_exec( List **list, char **envp )
{
	int pid;
	int status = 0;

	if ( !(*list)->data[0] )
	{
		*list = (*list)->next;
		return (0);
	}
	if ( strcmp((*list)->data[0], "cd") == 0 )
		return ( _cd(list) );
	if ( (pid = fork()) < 0 )
		exit_fatal();
	if ( pid == 0 )
	{
		execve((*list)->data[0], (*list)->data, envp);
		exit_cmd_error((*list)->data[0]);
	}
	waitpid(pid, &status, WUNTRACED);
	*list = (*list)->next;
	return (status);
}

void	start_pipe( List *list, char **envp, int *fds )
{
	int pid;

	if ( (pipe(fds)) < 0 )
		exit_fatal();
	pid = fork();
	if ( pid < 0 )
		exit_fatal();
	else if ( pid == 0 )
	{
		dup2(fds[1], 1); // we take from stdin here
		close(fds[1]);
		close(fds[0]);
		execve( list->data[0], list->data, envp );
		exit_cmd_error(list->data[0]);
	}
	close(fds[1]);
}

void	mid_pipe( List *list, char **envp, int *fds )
{
	int pid;
	int	old_in = fds[0];

	if ( (pipe(fds)) < 0 )
		exit_fatal();
	pid = fork();
	if ( pid < 0 )
		exit_fatal();
	else if ( pid == 0 )
	{
		dup2(old_in, 0);
		dup2(fds[1], 1);
		close(old_in);
		close(fds[0]);
		close(fds[1]);
		execve( list->data[0], list->data, envp );
		exit_cmd_error(list->data[0]);
	}
	close(old_in);
	close(fds[1]);
}

int		end_pipe( List *list, char **envp, int *fds )
{
	int pid = fork();

	if (pid < 0)
		exit_fatal();
	else if ( pid == 0 )
	{
		dup2(fds[0], 0);
		close(fds[0]);
		close(fds[1]);
		execve(list->data[0], list->data, envp);
		exit_cmd_error(list->data[0]);
	}
	close(fds[0]);
	return pid;
}

int		pipe_exec( List **list, char **envp )
{
	int pid, status, fds[2];

	start_pipe(*list, envp, fds);
	*list = (*list)->next;
	while ( (*list)->type == PIPE )
	{
		mid_pipe(*list, envp, fds);
		*list = (*list)->next;
	}
	pid = end_pipe(*list, envp, fds);
	*list = (*list)->next;
	status = 0;
	waitpid(pid, &status, WUNTRACED);
	return (status);
}

int		exec_cmd( List *list, char **envp )
{
	int ret;

	while (list)
	{
		if (list->type != PIPE)
			ret = simple_exec( &list, envp );
		else
			ret = pipe_exec( &list, envp );
	}
	clear_list(g_list);
	return (ret);
}

int		main( int ac, char **av, char **envp )
{
	List list;

	if (ac == 1)
		return 0;
	parser( &list, av, ac );
	g_list = list.next;
	return (exec_cmd(list.next, envp));
}
