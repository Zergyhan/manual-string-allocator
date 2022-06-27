# Rapport

Pour commencer, au debut du projet, avant d'avoir meme ajoute la premiere ligne 
de code, nous nous somme renseignes sur les plusieurs types d'allocation de memoire
possible. A la fin, pour ce projet, nous avons choisi 2 styles. Une avec des sizes fixes
pour les structures, parce que leur sizeof() ne change pas. L'autre style etait une avec 
un linked list qui pointe aux areas de memoire ouvertes. Ces deux ensembles 
fonctionnent bient ensemble. Il y avait l'option de reimplementer ptmalloc2 ou
quelque chose de ce style, mais c'etait trop complique pour ce projet, et c'est plus
efficace avec des styles avec des ints, floats, etc et non juste une structure fixte
et des strings super variable en largeur. 

C lui-meme n'etait pas tres difficile a utiliser, mais il faut toujours faire
attention de ne pas implementer de bugs, car c'est tres facile de faire un
misstep et se tirer dans le pied. Le debugger en CLion fonctionne super bien et
a vraiment aider a debugger le programme lorsqu'on avait de besoin. Ca a pris un
peu pour comprendre qu'il fallait avoir -d -O0 comme CFLAGS extra, mais apres ca
c'etait parfaitement fine. Regarder en memoire les pointeurs aussi a vraiment aide
a savoir ce que les bugs faisait et les regler. Une chose qui a aide aussi
c'est de garder un bout de papier avec un pen sur le cote pour dessiner et 
ecrire ses problemes sur papier. Une chose qui a aussi cause un peu de problemes
au debut c'est de toujours caster au bon pointeur avant d'avancer par un certain nombre,
car il depend sur le sizeof() du type. Des fois, a cause de ca, on multipliait par
accident et les offsets etaient mals calcules, qui fait des segmentation faults,
modife des donnes qu'il n'etait pas suppose en memoire, et faire en sorte qu'il
n'y a rien a la place qu'on se rend aussi. Apres se regler ca, c'etait pas trop
difficile de s'en rendre compte apres.



### Annexe:

Ce system utilise 2 headers, un pour stocker les string structs, et un pour
stocker les characteres des strings. Les headers pointent vers des pages qui
eux augmentent de 2^index a chaque fois. Ex: 4096B, 8192B, 16384B, 32768B, etc.
Ceci fait en sorte que nous pouvons gerer beaucoup de memoire dans un seul header.


A la base, le system utilise sysconf(_SC_PAGESIZE) bytes comme header de base.
Sur un systeme normal, cela correspond à 4096 bytes. 64 bit prend 8 bytes 
pour 1 pointer, alors nous pouvons pointer a 512 pages differentes pour le 
String struct lui meme et pour le data. Alors jusqu'a 5e157 bytes de data
peut etre stocke.

Chaque (size_t *) du header pointe vers une page si elle est initialisee et NULL
sinon.

La premiere sorte de page que nous allons discuter de est la page de String structs.
Le premier size_t dans cette page contient le nombre de String structs qui peuvent etre
stockes. Ensuite, les prochains words, en fonction du nombre de String structs qui 
peuvent etre stockes, contient des flags qui indiquent si la case est utilise par un 
String ou non. L'allocation et la desallocation se fait en flippant ce bit. 
Les indexs sont calcules par le nombre de words utilises pour des 
flags * sizeof(size_t) * 8 + # du bit. Apres, tu peux aller a la fin du metadata 
en calculant ca termine ou, et apres sauter (String *) index. Ici, tu peux facilement
faire tes str-> pour acceder aux donnes. Dans celui-ci on a allocated size, au cas ou
que ca differe du actual size, on a size, le size requested, char *data, dans quel page
de string structs qu'il est et finalement dans quelle page de data le char *data pointe vers.
Il y a tres peux de overhead pour le metadata, mais ca vient avec un cout en 
efficacite de temps. 

La deuxieme sorte de page est la page de data. Celle-ci est tres facile compare, car 
elle alloue des sizes variables. L'entree est simplement un pointeur (size_t *) qui
pointe vers la premiere place de libre (Linked list) dans la page. Ce qu'elle pointe
en specifique est un autre pointeur vers la prochaine place du linked list. Le (size_t)
juste a cote du pointeur est combien de bytes de libre pour ce node. Malheureusement, 
l'amalgamage n'est pas implemente, donc il faut faire str_compact() pour le meme effet.

Nous ne stockons pas combien de bytes chaque page est, car sa poisition dans le header
correspond a celle-ci (sysconf(_SC_PAGESIZE) * 2^index).

Stockage d'une chaine de taille N: N Bytes (minimum 16) + sizeof(String)
Ceux-ci contienent aussi 8 bytes pour le head pointer, 8 pour le number of strings dans 
la page String, et ceil_size_t((double) max_cells / (double) sizeof(size_t) * 8)
Ceci se fait en plus que O(1), mais moins que O(n), mais exactement jsp.

Liberation: O(1), toujours constant, tu fais juste reecrire le metadata et flip un bit.
La compactation prend un temps O(n) avec le nombre de strings a compacter.

Fragmentation est un gros probleme, seulement pour les char*, car il n'y a pas d'amalgamage. 
Ca fait en sorte que un bloc reste d'une certaine largeur apres etre alloue.
Si j'avais assez de temps pour implementer un system qui fonctionnait, ca ne serait plus
un gros probleme.