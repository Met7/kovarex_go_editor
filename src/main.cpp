// Go Editor -- for recording, editing and annotating games of Go, in SGF.
//
// Moves, variations, set-up stones, every kind of mark SGF has, comments and
// game information, with two edits beyond the usual: a stone can be dragged
// to where it should have been played, however many moves ago, and a
// forgotten exchange can be inserted into the middle of a game with
// Ctrl+click. The keyboard shortcuts work wherever the focus is.
//
//   game/    SGF, the rules and the record being edited -- no window, no input,
//            no drawing
//   ui/      everything on screen: the board, the side panel, the pages, the
//            shortcuts, and the Agui backend and theme they are built from
//   app/     the window, the main loop, files, settings, and the few things
//            only Windows can do (reading charsets, the .sgf association)
//
// Double-clicking a .sgf file opens it here once Settings > "Open them with
// this program" has been pressed; the file arrives on the command line.

#include <app/App.hpp>
#include <app/Platform.hpp>

int main()
{
  App app(platform::CommandLineFiles());
  return app.run();
}
