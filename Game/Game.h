#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        // Засекаем время начала игры
        auto start = chrono::steady_clock::now();

        // Если это повтор игры (REPLAY), перезагружаем конфигурацию и обновляем игровое поле
        if (is_replay)
        {
            logic = Logic(&board, &config);  // Создаём новый объект логики игры
            config.reload();                 // Перезагружаем конфиг
            board.redraw();                   // Перерисовываем доску
        }
        else
        {
            board.start_draw();  // Начальная отрисовка доски
        }

        is_replay = false; // Сбрасываем флаг повтора игры

        int turn_num = -1;  // Номер текущего хода
        bool is_quit = false;  // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns");  // Получаем максимальное количество ходов из конфига

        // 🔄 Основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;  // Обнуляем серию захватов

            // Находим доступные ходы для игрока (turn_num % 2 определяет, чей сейчас ход: 0 - белые, 1 - чёрные)
            logic.find_turns(turn_num % 2);

            // Если нет доступных ходов — выход из цикла
            if (logic.turns.empty())
                break;

            // Получаем уровень сложности бота из конфига
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Проверяем, управляется ли данный цвет игроком или ботом
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Если ход делает игрок
                auto resp = player_turn(turn_num % 2);

                if (resp == Response::QUIT)  // Выход из игры
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Запрос на повтор игры
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Откат хода назад
                {
                    // Если бот сделал ход, откатываем два хода назад (игрока и бота)
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }

                    // Если не было захвата, просто откатываем один ход назад
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
            {
                // Если ход делает бот
                bot_turn(turn_num % 2);
            }
        }

        // Засекаем время окончания игры
        auto end = chrono::steady_clock::now();

        // Логируем время игры в файл
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если был запрос на повтор игры — запускаем её заново
        if (is_replay)
            return play();

        // Если игрок вышел из игры, возвращаем 0
        if (is_quit)
            return 0;

        // Определяем результат игры
        int res = 2; // 2 - ничья или незавершённая игра
        if (turn_num == Max_turns)
        {
            res = 0; // Ничья
        }
        else if (turn_num % 2)
        {
            res = 1; // Победа одного из игроков
        }

        // Показываем финальный экран
        board.show_final(res);

        // Ожидаем реакции игрока (например, хочет ли он сыграть ещё раз)
        auto resp = hand.wait();

        // Если игрок хочет повторить игру — запускаем её заново
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }

        return res; // Возвращаем результат игры
    }

  private:
    void bot_turn(const bool color)
    {
        // Засекаем время начала хода бота
        auto start = chrono::steady_clock::now();

        // Получаем задержку в миллисекундах между ходами бота из конфигурации
        auto delay_ms = config("Bot", "BotDelayMS");

        // Запускаем новый поток, чтобы добавить задержку для бота. Это позволяет каждому ходу бота задерживаться на заданное время.
        thread th(SDL_Delay, delay_ms);

        // Находим лучшие ходы для бота с учетом его цвета (цвет передается как параметр)
        auto turns = logic.find_best_turns(color);

        // Ожидаем завершения потока с задержкой
        th.join();

        bool is_first = true;
        // Применяем ходы бота
        for (auto turn : turns)
        {
            // Для каждого хода (кроме первого) добавляем задержку
            if (!is_first)
            {
                SDL_Delay(delay_ms);  // Задержка между ходами бота
            }

            // После первого хода, флаг is_first будет false
            is_first = false;

            // Увеличиваем серию "побежденных" фигур, если в ходе бота был съеденный фрагмент
            beat_series += (turn.xb != -1);

            // Выполняем ход бота на доске (передаем информацию о ходе и серии побежденных)
            board.move_piece(turn, beat_series);
        }

        // Засекаем время окончания хода бота
        auto end = chrono::steady_clock::now();

        // Записываем время, которое бот потратил на этот ход, в лог-файл
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // Подсвечиваем возможные ходы для текущего игрока
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells); // Подсветка возможных ходов

        // Инициализация переменных для хранения текущего хода
        move_pos pos = { -1, -1, -1, -1 };
        POS_T x = -1, y = -1;

        // Ожидаем первый ход игрока
        while (true)
        {
            auto resp = hand.get_cell(); // Получаем ввод от игрока
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp); // Если не клетка, возвращаем ответ (например, QUIT или REPLAY)

            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) }; // Получаем координаты клетки

            // Проверяем, является ли клетка допустимой для хода
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{ x, y, cell.first, cell.second })
                {
                    pos = turn;
                    break;
                }
            }

            // Если выбран корректный ход — выходим из цикла
            if (pos.x != -1)
                break;

            // Если клетка недоступна, сбрасываем выбор и ждём нового ввода
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();   // Сбрасываем активную клетку
                    board.clear_highlight(); // Сбрасываем подсветку
                    board.highlight_cells(cells); // Подсвечиваем доступные клетки заново
                }
                x = -1;
                y = -1;
                continue;
            }

            // Если клетка корректная — сохраняем её и подсвечиваем возможные дальнейшие ходы
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);

            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2); // Подсветка возможных следующих ходов
        }

        // Очищаем подсветку и перемещаем фигуру
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);

        // Если фигура не бьёт другую, завершаем ход
        if (pos.xb == -1)
            return Response::OK;

        // Если был выполнен удар, продолжаем серию взятий (beat_series)
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2); // Ищем возможные последующие взятия
            if (!logic.have_beats)
                break; // Если больше нельзя бить, выходим из цикла

            // Подсвечиваем клетки, доступные для следующего удара
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);

            // Ожидание выбора клетки для следующего удара
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp); // Если не клетка, возвращаем соответствующий ответ

                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

                // Проверяем, является ли выбранный ход корректным
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }

                // Если ход некорректен — просим выбрать снова
                if (!is_correct)
                    continue;

                // Очищаем подсветку, обновляем поле и выполняем ход
                board.clear_highlight();
                board.clear_active();
                beat_series += 1; // Увеличиваем серию взятий
                board.move_piece(pos, beat_series);
                break; // Выходим из цикла ожидания
            }
        }

        return Response::OK; // Завершаем ход
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
