#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color)
    {
        next_best_state.clear();
        next_move.clear();

        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            res.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1);
        return res;
    }

private:
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если был побежден противник, очищаем соответствующую клетку
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;  // Убираем побежденную фигуру с доски

        // Если фигура достигла своей последней линии (для белых - 0, для черных - 7), превращаем ее в дамку
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;  // Превращаем фигуру в дамку (прибавляем 2 к значению)

        // Перемещаем фигуру из исходной клетки в целевую
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];  // Ставим фигуру на новое место

        // Очищаем исходную клетку
        mtx[turn.x][turn.y] = 0;  // Убираем фигуру с исходной клетки

        return mtx;  // Возвращаем измененную доску
    }

    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // Инициализация переменных для подсчета количества фигур каждого типа
        double w = 0, wq = 0, b = 0, bq = 0;

        // Проход по всей доске (матрице) для подсчета количества белых и черных фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Подсчет обычных белых фигур (тип 1)
                w += (mtx[i][j] == 1);
                // Подсчет дамок белых фигур (тип 3)
                wq += (mtx[i][j] == 3);
                // Подсчет обычных черных фигур (тип 2)
                b += (mtx[i][j] == 2);
                // Подсчет дамок черных фигур (тип 4)
                bq += (mtx[i][j] == 4);

                // Если режим подсчета учитывает потенциал, добавляем бонус в зависимости от положения фигуры
                if (scoring_mode == "NumberAndPotential")
                {
                    // Бонус для белых фигур в зависимости от их строки (чем выше, тем лучше)
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    // Бонус для черных фигур в зависимости от их строки (чем ниже, тем лучше)
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }

        // Если цвет первого бота не совпадает с цветом максимизирующего игрока, меняем местами оценки
        if (!first_bot_color)
        {
            swap(b, w);   // Меняем количество черных и белых фигур местами
            swap(bq, wq); // Меняем количество дамок черных и белых местами
        }

        // Если на доске нет белых фигур (обычных или дамок), возвращаем инфинити, чтобы это состояние было максимально худшим
        if (w + wq == 0)
            return INF;

        // Если на доске нет черных фигур (обычных или дамок), возвращаем 0, что означает победу белых
        if (b + bq == 0)
            return 0;

        // Коэффициент для дамок (по умолчанию 4, если используется режим "NumberAndPotential", то 5)
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;  // Более высокий коэффициент для дамок в режиме с учетом потенциала
        }

        // Возвращаем оценку в виде отношения количества черных фигур и дамок к количеству белых фигур и дамок,
        // где дамки учитываются с учетом коэффициента
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        // Добавляем базовые значения в векторы
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);

        double best_score = -1;

        // Если это не первый шаг, ищем возможные ходы
        if (state != 0)
        {
            find_turns(x, y, mtx);
        }

        auto current_turns = turns;
        bool has_beats = have_beats;

        // Если нет обязательных взятий и это не первый ход, переходим к рекурсивному поиску
        if (!has_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Перебираем все возможные ходы
        for (const auto& turn : current_turns)
        {
            size_t next_state = next_move.size();
            double score;

            // Если есть взятие, продолжаем ход для той же стороны
            if (has_beats)
            {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else // Иначе передаём ход противнику
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Обновляем лучший найденный результат
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = has_beats ? static_cast<int>(next_state) : -1;
                next_move[state] = turn;
            }
        }

        return best_score;
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Если достигнута максимальная глубина рекурсии, оцениваем положение
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Если указан конкретный ход, ищем возможные ходы для него
        if (x != -1)
        {
            find_turns(x, y, mtx);
        }
        else // Иначе ищем ходы для текущего игрока
        {
            find_turns(color, mtx);
        }

        auto current_turns = turns;
        bool has_beats = have_beats;

        // Если нет обязательных взятий и это не первый ход, передаем ход противнику
        if (!has_beats && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если ходов нет, возвращаем крайнее значение (проигрыш или выигрыш)
        if (turns.empty())
        {
            return (depth % 2 ? 0 : INF);
        }

        double min_score = INF + 1;
        double max_score = -1;

        // Перебираем все возможные ходы
        for (const auto& turn : current_turns)
        {
            double score = 0.0;

            // Если нет взятия и это первый ход в данной ветке, передаём ход противнику
            if (!has_beats && x == -1)
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else // Иначе продолжаем ход для текущего игрока
            {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }

            // Обновляем минимальный и максимальный найденные результаты
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Alpha-beta отсечение
            if (depth % 2) // Максимизирующий игрок
            {
                alpha = max(alpha, max_score);
            }
            else // Минимизирующий игрок
            {
                beta = min(beta, min_score);
            }

            // Если используется оптимизация и отсечение сработало, сразу возвращаем результат
            if (optimization != "O0" && alpha >= beta)
            {
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }

        // Возвращаем либо максимальный, либо минимальный результат в зависимости от уровня
        return (depth % 2 ? max_score : min_score);
    }


public:
    // Функция для поиска всех возможных ходов для бота или игрока по цвету
    void find_turns(const bool color)
    {
        // Вызов функции поиска ходов для определенной доски
        find_turns(color, board->get_board());
    }

    // Функция для поиска ходов для конкретной клетки (x, y)
    void find_turns(const POS_T x, const POS_T y)
    {
        // Вызов функции поиска ходов для клетки (x, y) на текущей доске
        find_turns(x, y, board->get_board());
    }

private:
    // Основная функция для поиска всех ходов на доске
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;  // Вектор для хранения найденных ходов
        bool have_beats_before = false;  // Флаг, указывающий, были ли ходы с побеждением ранее

        // Перебор всех клеток доски
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Если клетка занята фигурами противоположного цвета
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    // Находим ходы для этой фигуры
                    find_turns(i, j, mtx);

                    // Если были ходы с побеждением и это первый такой ход
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();  // Очищаем список ходов перед первым побеждением
                    }

                    // Если были ходы с побеждением, или нет побеждений вообще
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());  // Добавляем новые ходы в список
                    }
                }
            }
        }

        // Обновляем список возможных ходов
        turns = res_turns;

        // Перемешиваем ходы для случайного выбора (например, для бота)
        shuffle(turns.begin(), turns.end(), rand_eng);

        // Обновляем флаг, были ли ходы с побеждением
        have_beats = have_beats_before;
    }

    // Функция для поиска ходов для одной конкретной клетки (x, y)
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();  // Очищаем список ходов
        have_beats = false;  // Сбрасываем флаг побеждений

        POS_T type = mtx[x][y];  // Тип фигуры на клетке (например, обычная или ферзь)

        // Проверка возможных побеждений
        switch (type)
        {
        case 1:
        case 2:
            // Для обычных фигур (1 и 2)
            for (POS_T i = x - 2; i <= x + 2; i += 4)  // Перебор клеток по диагонали
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    // Проверяем, что клетка находится в пределах доски
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;

                    // Рассчитываем позицию побежденной фигуры
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;

                    // Проверка условий для хода с побеждением
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;

                    // Добавляем возможный ход с побеждением
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // Для ферзей
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;

                    // Перебор клеток по диагонали, пока не упремся в фигуру или границу доски
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        // Если в клетке есть фигура
                        if (mtx[i2][j2])
                        {
                            // Если фигура того же цвета или побежденная фигура уже есть, прерываем цикл
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;  // Сохраняем координаты побежденной фигуры
                            yb = j2;
                        }

                        // Если есть побежденная фигура, добавляем ход
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }

        // Если ходы с побеждением не найдены, проверяем другие возможные ходы
        if (!turns.empty())
        {
            have_beats = true;
            return;  // Если есть ходы с побеждением, возвращаем
        }

        // Для обычных фигур (1 и 2) проверяем возможные ходы без побеждений
        switch (type)
        {
        case 1:
        case 2:
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);  // Определяем направление хода
            for (POS_T j = y - 1; j <= y + 1; j += 2)
            {
                // Проверяем, что клетка внутри доски и пуста
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                    continue;

                // Добавляем возможный ход
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default:
            // Для ферзей проверяем возможные ходы по всем направлениям
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    // Перебор клеток по диагоналям, пока не упремся в фигуру или границу доски
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        // Если клетка занята фигурой, прерываем цикл
                        if (mtx[i2][j2])
                            break;

                        // Добавляем возможный ход
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }


 public:
     // Список возможных ходов для текущей позиции
     vector<move_pos> turns;
     // Флаг, который указывает, были ли выполнены побеждения в текущем ходе
     bool have_beats;
     // Максимальная глубина поиска для алгоритмов, таких как минимакс или альфа-бета отсечение
     int Max_depth;
private:
    // Генератор случайных чисел, используется для перемешивания ходов, а также для случайного выбора ходов
    default_random_engine rand_eng;
    // Режим оценки, который может определять, как будет вычисляться "оценка" текущего состояния (например, по количеству фигур или их потенциалу)
    string scoring_mode;
    // Оптимизация поиска, может содержать информацию о способах ускорения поиска или стратегии
    string optimization;
    // Следующий ход, который будет сделан в процессе игры, хранится как последовательность ходов
    vector<move_pos> next_move;
    // Оценка лучшего состояния, используемая для выбора хода (например, минимакс-оценка)
    vector<int> next_best_state;
    // Указатель на объект доски, представляющий текущую игровую доску
    Board* board;
    // Указатель на объект конфигурации, который хранит параметры игры, такие как настройки бота, скорости и другие параметры
    Config* config;
};
