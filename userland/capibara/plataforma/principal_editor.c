// SPDX-License-Identifier: MIT
/* principal_editor.c - el punto de entrada de SaguiOS del editor (D460): los programas empiezan en principal(argc, argv). */
int editor_principal(int argc, char **argv);
int principal(int argc, char **argv) { return editor_principal(argc, argv); }
