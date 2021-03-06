#include "Console.h"
#include <sstream>
#ifndef WIN32 // Linux
#include <curses.h>
#endif

TuringConsole::TuringConsole(std::ifstream& _code_file)
    : turing_position(0), current_code_line(0), code_file(_code_file)
#ifdef WIN32
    , console_info({})
#endif
{
#ifdef WIN32
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        dbg_error("Bad Handle Read. ErrorCode " << GetLastError());

    // TODO: Get Console info does not work in Clion
    if (GetConsoleScreenBufferInfo(handle, &console_info) > 0)
        dbg_println("Successfully obtained OutputConsole console_info.");
    else
        dbg_error("Failed to obtain OutputConsole console_info. ErrorCode " << GetLastError());

    // TODO: add windows event for when console size changes
    width = console_info.dwSize.X;
    height = console_info.dwSize.Y;
    dbg_println("Console Width:  " << width);
    dbg_println("Console Height: " << height);
    system("cls");
#else
    initscr();
    start_color();
    cbreak();
    noecho();

    // TODO: init color pairs without setting both Foreground and Background at the same time
    // TODO: no transparent color??
    init_pair(UNACTIVE_SCROLL, COLOR_BLACK, COLOR_LIGHT_BLACK);
    init_pair(ACTIVE_SCROLL, COLOR_BLACK, COLOR_WHITE);
    init_pair(ACTIVE_CODE_LINE, COLOR_BLACK, COLOR_GREEN);
    init_pair(TAPE_CURSOR, COLOR_BLACK, COLOR_CYAN);
    init_pair(COMMENT_LINE, COLOR_LIGHT_BLACK, COLOR_BLACK);
    init_pair(INSTRUCTION_KEY, COLOR_YELLOW, COLOR_BLACK);
    init_pair(INSTRUCTION_TXT, COLOR_GREEN, COLOR_BLACK);

    width = COLS;
    height = LINES;
#endif

    tape_display_width = width - 10;

    // setting up the Turing Tape
    // margin: 0 1 0 1

    /* scroller-btns
         width:  3
         height: 3
         bg-col: white (47)
         &:disabled
           bg-col: light_black (100)
    */
    draw_tape_scrollers();
    print_instructions();
}

#ifndef WIN32 // Linux
    TuringConsole::~TuringConsole() { endwin(); }
#endif


#if WIN32
void TuringConsole::set_color(color col)
{
    // ANSI Colors
    std::cout << "\033[" << (unsigned int)col << "m";
}
#endif

void TuringConsole::set_tape_cursor(unsigned short position, const std::string& tape)
{
#ifdef WIN32
    set_position({ (unsigned short)(tape_display_start.x + turing_position), tape_display_start.y });
    set_color(color::reset);
    std::cout << tape[turing_position];
#else
    mvaddch(tape_display_start.y, tape_display_start.x + turing_position, tape[turing_position]);
#endif

    // Highlight new position
#ifdef WIN32
    set_position({ (unsigned short)(tape_display_start.x + position), tape_display_start.y });
    set_color(color::cyan_bg);
    std::cout << tape[position];
    set_color(color::reset);
#else
    attron(COLOR_PAIR(TAPE_CURSOR));
    mvaddch(tape_display_start.y, tape_display_start.x + position, tape[position]);
    attroff(COLOR_PAIR(TAPE_CURSOR));
    refresh();
#endif

    turing_position = position;
}

void TuringConsole::set_position(coord pos) // NOLINT(readability-convert-member-functions-to-static)
{
#if WIN32
    SetConsoleCursorPosition(handle, COORD{ (short)pos.x, (short)pos.y });
#else
    // NOTE: for ncurses, the x and y are reversed (y comes first)
    move(static_cast<int>(pos.y), static_cast<int>(pos.x));
#endif
}

void TuringConsole::set_current_code_line(unsigned short line, std::ifstream& file)
{
    // start from the beginning
    file.clear();
    file.seekg(0);

    // First line is line 1
    unsigned short line_count = 0;
    // completed resetting the current_code_line
    bool resetted = false;
    // Completed highlighting the line
    bool colored = false;

    if (file.is_open())
    {
        // Stop when both tasks (resetting current_line and highlighting target line) are complete
        while (file.good() && !(resetted && colored))
        {
            std::string s_line;
            std::getline(file, s_line);
            // First line is line 1
            line_count++;

            // Reset color of current_line
            if (line_count == current_code_line)
            {
#ifdef WIN32
                set_position({ code_start.x, (unsigned short)(line_count + code_start.y) });
                set_color(color::reset);
                std::cout << s_line;
#else
                mvaddstr(line_count + code_start.y, code_start.x, s_line.c_str());
#endif
                resetted = true;
            }
            // line and current_code_line could be the same, so no elseif
            
            // Set color of target line
            if (line_count == line)
            {
#ifdef WIN32
                set_position({ code_start.x, (unsigned short)(line_count + code_start.y) });
                set_color(color::green_bg);
                std::cout << s_line;
                set_color(color::reset);
#else
                attron(COLOR_PAIR(ACTIVE_CODE_LINE));
                mvaddstr(line_count + code_start.y, code_start.x, s_line.c_str());
                attroff(COLOR_PAIR(ACTIVE_CODE_LINE));
#endif
                colored = true;
            }
        }

        // target line is greater than the number of lines in the file
        if (!colored)
            std::cerr << "Argument <line> is greater than lines in the file" << std::endl;
        // Can safely set current_code_line
        else
            current_code_line = line;
    }
    else
        std::cerr << "Error opening file containing Turing instructions" << std::endl;

#ifndef WIN32 // Linux
    refresh();
#endif
    file.clear();
    file.seekg(0); 
}

void TuringConsole::write_at(char symbol, unsigned short tape_position)
{
#ifdef WIN32
    set_position({ (unsigned short)(tape_display_start.x + tape_position), tape_display_start.y });
    if (turing_position == tape_position)
        set_color(color::cyan_bg);
    std::cout << symbol;
    set_color(color::reset);
#else
    if (turing_position == tape_position)
        attron(COLOR_PAIR(TAPE_CURSOR));
    mvaddch(tape_display_start.y, tape_display_start.x + tape_position, symbol);
    attroff(COLOR_PAIR(TAPE_CURSOR));
    refresh();
#endif
}

void TuringConsole::draw_tape_scrollers(bool arrow1_disabled, bool arrow2_disabled) // NOLINT(readability-make-member-function-const)
{
#ifndef WIN32 // Linux
    short arrow1_attr, arrow2_attr;
    int arrow_right_x = width - 1 - 3;

    if (arrow1_disabled)
        arrow1_attr = UNACTIVE_SCROLL;
    else
        arrow1_attr = ACTIVE_SCROLL;
    if (arrow2_disabled)
        arrow2_attr = UNACTIVE_SCROLL;
    else
        arrow2_attr = ACTIVE_SCROLL;
#endif

    // Left Scroller Arrow
#ifdef WIN32
    set_color(color::black_fg);
    if (arrow1_disabled)
        set_color(color::light_black_bg);
    else
        set_color(color::white_bg);
    set_position({ 1, 1 });
    std::cout << "   ";
    set_position({ 1, 2 });
    std::cout << " < ";
    set_position({ 1, 3 });
    std::cout << "   ";
#else
    attron(COLOR_PAIR(arrow1_attr));
    mvaddstr(1, 1, "   ");
    mvaddstr(2, 1, " < ");
    mvaddstr(3, 1, "   ");
    attroff(COLOR_PAIR(arrow1_attr));
#endif

    // Right Scroller Arrow
#ifdef WIN32
    if (arrow2_disabled)
        set_color(color::light_black_bg);
    else
        set_color(color::white_bg);

    auto arrow_right_x = (unsigned short)(width - 1 - 3);

    set_position({ arrow_right_x, 1 });
    std::cout << "   ";
    set_position({ arrow_right_x, 2 });
    std::cout << " > ";
    set_position({ arrow_right_x, 3 });
    std::cout << "   ";

    set_color(color::reset);
#else
    attron(COLOR_PAIR(arrow2_attr));
    mvaddstr(1, arrow_right_x, "   ");
    mvaddstr(2, arrow_right_x, " > ");
    mvaddstr(3, arrow_right_x, "   ");
    attroff(COLOR_PAIR(arrow2_attr));
    refresh();
#endif
}

void TuringConsole::set_tape_value(const std::string& tape)
{
    set_position(tape_display_start);

    if (tape.size() > tape_display_width)
        ; // TODO: note: tape cursor (cyan) should be in the middle.
    else
        for (unsigned int i = 0; i < tape.size(); i++)
            if (i == turing_position)
            {
#ifdef WIN32
                set_color(color::cyan_bg);
                std::cout << tape[i];
                set_color(color::reset);
#else
                attron(COLOR_PAIR(TAPE_CURSOR));
                addch(tape[i]);
                attroff(COLOR_PAIR(TAPE_CURSOR));
#endif
            }
            else
            {
#ifdef WIN32
                std::cout << tape[i];
#else
                addch(tape[i]);
#endif
            }

#ifndef WIN32 // Linux
    refresh();
#endif
}

bool TuringConsole::print_turing_code(std::ifstream& file)
{
    char c;
    // TODO: cannot print past last line in ncurses/linux

    set_position(code_start);
    if (file.is_open())
        while (file.good())
        {
            c = file.get();

#ifdef WIN32
            // Comment
            if (c == ';')
                set_color(color::light_black_fg);
            else if (c == '\n')
                set_color(color::reset);

            std::cout << c;
#else
            // Comment
            if (c == ';')
                attron(COLOR_PAIR(COMMENT_LINE));
            else if (c == '\n')
                attroff(COLOR_PAIR(COMMENT_LINE));

            addch(c);
#endif
        }
    else
    {
        std::cerr << "Error opening file containing Turing instructions" << std::endl;
        return false;
    }

    file.clear();
    file.seekg(0);
#ifdef WIN32
    set_color(color::reset);
#else
    attroff(COLOR_PAIR(COMMENT_LINE));
    refresh();
#endif

    return true;
}

void TuringConsole::print_instructions()
{
    set_position({ 5, 5 });
#ifdef WIN32
    set_color(color::yellow_fg);
    std::cout << "<-";
    set_color(color::green_fg);
    std::cout << " | ";
    set_color(color::yellow_fg);
    std::cout << "->";
    set_color(color::green_fg);
    std::cout << " : Scroll Tape   ";
    set_color(color::yellow_fg);
    std::cout << "v";
    set_color(color::green_fg);
    std::cout << " | ";
    set_color(color::yellow_fg);
    std::cout << "^";
    set_color(color::green_fg);
    std::cout << " : Scroll Code   ";
    set_color(color::yellow_fg);
    std::cout << "F10";
    set_color(color::green_fg);
    std::cout << " : Step";

    set_color(color::reset);
#else
    attron(COLOR_PAIR(INSTRUCTION_KEY));
    addstr("<-");
    attron(COLOR_PAIR(INSTRUCTION_TXT));
    addstr(" | ");
    attron(COLOR_PAIR(INSTRUCTION_KEY));
    addstr("->");
    attron(COLOR_PAIR(INSTRUCTION_TXT));
    addstr(" : Scroll Tape   ");
    attron(COLOR_PAIR(INSTRUCTION_KEY));
    addstr("v");
    attron(COLOR_PAIR(INSTRUCTION_TXT));
    addstr(" | ");
    attron(COLOR_PAIR(INSTRUCTION_KEY));
    addstr("^");
    attron(COLOR_PAIR(INSTRUCTION_TXT));
    addstr(" : Scroll Code   ");
    attron(COLOR_PAIR(INSTRUCTION_KEY));
    addstr("F10");
    attron(COLOR_PAIR(INSTRUCTION_TXT));
    addstr(" : Step");
    
    attroff(COLOR_PAIR(INSTRUCTION_KEY));
    attroff(COLOR_PAIR(INSTRUCTION_TXT));
    refresh();
#endif
}