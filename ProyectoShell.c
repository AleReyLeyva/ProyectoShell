/*------------------------------------------------------------------------------
Proyecto Shell de UNIX. Sistemas Operativos
Grados I. Inform�tica, Computadores & Software
Dept. Arquitectura de Computadores - UMA
Autor: Alejandro Rey Leyva - Matematicas + Informatica

Algunas secciones est�n inspiradas en ejercicios publicados en el libro
"Fundamentos de Sistemas Operativos", Silberschatz et al.

Para compilar este programa: gcc ProyectoShell.c ApoyoTareas.c -o MiShell
Para ejecutar este programa: ./MiShell
Para salir del programa en ejecuci�n, pulsar Control+D
------------------------------------------------------------------------------*/

#include "ApoyoTareas.h" // Cabecera del m�dulo de apoyo ApoyoTareas.c

#define MAX_LINE 256 // 256 caracteres por l�nea para cada comando es suficiente
#include <string.h>  // Para comparar cadenas de cars. (a partir de la tarea 2)

job *todos;

void manejador()
{

  job *todo;
  int pid_todo, pid_wait, status, info;
  enum status status_res;

  for (int i = 1; i <= list_size(todos); i++)
  {
    block_SIGCHLD();
    todo = get_item_bypos(todos, i);
    unblock_SIGCHLD();
    pid_wait = waitpid(todo->pgid, &status, WUNTRACED | WNOHANG);
    status_res = analyze_status(status, &info);
    if (status_res == FINALIZADO)
    {
      printf("\nComando %s ejecutado en %s con PID %d ha concluido.\n",
             todo->command,
             ground_strings[todo->ground],
             pid_wait);
      block_SIGCHLD();
      delete_job(todos, todo);
      unblock_SIGCHLD();
    }
    else if (status_res == SUSPENDIDO)
    {
      printf("\nComando %s ejecutado en %s con PID %d ha suspendido su ejecucion.\n",
             todo->command,
             ground_strings[todo->ground],
             pid_wait);
      block_SIGCHLD();
      todo->ground = DETENIDO;
      unblock_SIGCHLD();
    }
  }
}

// --------------------------------------------
//                     MAIN
// --------------------------------------------

int main(void)
{
  char inputBuffer[MAX_LINE]; // B�fer que alberga el comando introducido
  int background;             // Vale 1 si el comando introducido finaliza con '&'
  char *args[MAX_LINE / 2];   // La l�nea de comandos (de 256 cars.) tiene 128 argumentos como m�x
                              // Variables de utilidad:
  int pid_fork, pid_wait;     // pid para el proceso creado y esperado
  int status;                 // Estado que devuelve la funci�n wait
  enum status status_res;     // Estado procesado por analyze_status()
  int info;                   // Informaci�n procesada por analyze_status()
  job *todo;
  ignore_terminal_signals();
  signal(SIGCHLD, manejador);

  todos = new_list("TODOS");

  while (1) // El programa termina cuando se pulsa Control+D dentro de get_command()
  {
    printf("COMANDO-> ");
    fflush(stdout);
    get_command(inputBuffer, MAX_LINE, args, &background); // Obtener el pr�ximo comando
    if (args[0] == NULL)
      continue; // Si se introduce un comando vac�o, no hacemos nada

    //----------------------------------- cd --------------------------------------

    if (strcmp(args[0], "cd") == 0)
    {
      if (args[1] == NULL)
      {
        chdir("/home");
        chdir(getenv("USERNAME"));
      }
      else if (chdir(args[1]) == -1)
      {
        printf("No se puede cambiar al directorio %s \n \n", args[1]);
      }
      continue;
    }

    //----------------------------------- logout --------------------------------------

    if (strcmp(args[0], "logout") == 0)
    {
      printf("Saliendo del Shell \n");
      exit(0);
    }

    //----------------------------------- jobs --------------------------------------

    if (strcmp(args[0], "jobs") == 0)
    {
      block_SIGCHLD();
      if (list_size(todos) == 0)
      {
        printf("La lista de tareas esta vacia \n \n");
      }
      else
      {
        print_job_list(todos);
      }
      unblock_SIGCHLD();
      continue;
    }

    //----------------------------------- foreground --------------------------------------

    if (strcmp(args[0], "fg") == 0)
    {
      int i = 1;
      if (args[1] != NULL)
      {
        i = atoi(args[1]);
      }

      block_SIGCHLD();
      todo = get_item_bypos(todos, i);
      if (todo == NULL)
      {
        printf("La lista de tareas esta vacia o el indice indicado esta fuera de rango \n \n");
        continue;
      }
      set_terminal(todo->pgid);
      if (todo->ground == DETENIDO)
      {
        killpg(todo->pgid, SIGCONT);
      }
      pid_fork = todo->pgid;
      delete_job(todos, todo);
      unblock_SIGCHLD();
    }

    //----------------------------------- background --------------------------------------

    if (strcmp(args[0], "bg") == 0)
    {
      int i = 1;
      if (args[1] != NULL)
      {
        i = atoi(args[1]);
      }
      block_SIGCHLD();
      job *todo = get_item_bypos(todos, i);
      if (todo == NULL)
      {
        printf("La lista de tareas esta vacia o el indice indicado esta fuera de rango \n \n");
        continue;
      }
      if (todo->ground == DETENIDO)
      {
        killpg(todo->pgid, SIGCONT);
        todo->ground = SEGUNDOPLANO;
      }
      unblock_SIGCHLD();
      continue;
    }

    //----------------------------------- comandos externos ----------------------------------

    if (!strcmp(args[0], "fg") == 0)
      pid_fork = fork();

    if (pid_fork == 0)
    {
      new_process_group(getpid());
      if (background == 0)
      {
        set_terminal(getpid());
      }
      restore_terminal_signals();
      execvp(args[0], args);
      printf("Error. Comando %s no encontrado. \n", args[0]);
      exit(-1);
    }
    else
    {
      if (background == 0)
      {
        pid_wait = waitpid(pid_fork, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);
        if (status_res == FINALIZADO && info != 255)
        {
          printf(
              "\nComando %s ejecutado en primer plano con pid %d. Estado %s. Info: %d.\n\n",
              args[0],
              pid_wait,
              status_strings[status_res],
              info);
        }
        else if (status_res == SUSPENDIDO)
        {
          printf(
              "Comando %s ejecutado en primer plano con pid %d. Estado %s. Info: %d. \n\n",
              args[0],
              pid_wait,
              status_strings[status_res],
              info);
          block_SIGCHLD();
          todo = new_job(pid_fork, args[0], DETENIDO);
          add_job(todos, todo);
          unblock_SIGCHLD();
        }
      }
      else
      {
        printf("Comando %s ejecutado en segundo plano con pid %d. \n \n", args[0], pid_fork);
        block_SIGCHLD();
        todo = new_job(pid_fork, args[0], SEGUNDOPLANO);
        add_job(todos, todo);
        unblock_SIGCHLD();
      }
    }
  } // end while
}
