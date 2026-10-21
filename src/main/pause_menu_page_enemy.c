#include "common.h"

#include "pause_menu_page_enemy.h"


short DebugActor = 0;
static short Page_0 = 0;
const char D_004DB868[5] = "\310\257\270\253";
const char D_004DB870[7] = "\304\311\300\327\303\346";
const char D_004DB878[5] = "\271\305\304\276";
const char D_004DB880[5] = "\265\242\301\343";
const char D_004DB888[7] = "\277\250\274\352\303\346";
const char D_004DB890[5] = "\244\314\244\353";
const char D_004DB898[5] = "\277\250\274\352";
const char D_004DB8A0[7] = "\267\331\310\367\260\367";
const char D_004DB8A8[5] = "\314\347\310\326";
const char D_004DB8B0[7] = "\245\353\241\274\245\310";
const char D_004DB8B8[5] = "\261\375\311\374";
const char D_004DB8C0[3] = "\312\311";
const char D_004DB8C8[5] = "\311\342\315\267";
const char D_004DB8D0[5] = "\306\250\313\264";
const char D_004DB8D8[7] = "\244\314\244\353\241\241";
const char D_004DB8E0[7] = "\241\277\243\302\241\241";
const char D_004DB8E8[7] = "\241\277\243\324\241\241";
const char D_004DB8F0[7] = "\241\374\243\302\241\241";
const char D_004DB8F8[7] = "\242\243\243\302\303\346";
const char D_004DB900[7] = "\242\243\243\302\241\241";
const char D_004DB908[7] = "\242\243\243\324\241\241";
const char D_004DB910[7] = "\245\314\245\353\241\241";
const char D_004DB918[5] = "\313\314\242\254";
const char D_004DB920[5] = "\305\354\242\252";
const char D_004DB928[5] = "\306\356\242\255";
const char D_004DB930[5] = "\300\276\242\253";
const char D_004DB938[5] = "\304\314\276\357";

static inline int printEnemyCounters(int y, const char *format,
    int current, int limit)
{
    xglFontDebugPrintf(8, y, format, current, limit);
    return y + 12;
}

static inline void printEnemyFinalCounters(int y, const char *format,
    int current, int limit)
{
    xglFontDebugPrintf(8, y, format, current, limit);
}

static inline int printEnemyCounter(int y, const char *format, int count)
{
    xglFontDebugPrintf(8, y, format, count);
    return y + 12;
}

static inline int printEnemyAction(int y, int action, const char *label,
    int hit_count, int hit_limit)
{
    xglFontDebugPrintf(8, y, D_004CB390, action, label, hit_count, hit_limit);
    return y + 12;
}

static inline int printEnemyPause(int y, int pause_kind, const char *label)
{
    xglFontDebugPrintf(8, y, D_004CB378, pause_kind, label);
    return y + 12;
}

static inline int printEnemyFreeze(int y, int freeze_state, const char *label)
{
    xglFontDebugPrintf(8, y, D_004CB3B0, freeze_state, label);
    return y + 12;
}

static inline int printEnemyHiddenUnit(int y, int hidden_unit)
{
    xglFontDebugPrintf(8, y, D_004CB3E0, hidden_unit);
    return y + 12;
}

static inline int printEnemyFlag(int y, const char *format, int value,
    const char *label)
{
    xglFontDebugPrintf(8, y, format, value, label);
    return y + 12;
}

static inline int printEnemyLookAt(int y, const EnemyWork *enemy, const Actor *selected)
{
    const char *height_format = D_004CB4C8;
    double display_height = enemy->height;
    xglFontDebugPrintf(8, y, height_format, display_height,
        selected->look_at_state, selected->look_at_target);
    return y + 12;
}

static inline int printEnemyTerritoryID(int y, int index, unsigned long long id)
{
    xglFontDebugPrintf(8, y, D_004CB338, index, id);
    return y + 12;
}

static inline int printEnemyMark(int y, int mark, const char *label)
{
    xglFontDebugPrintf(8, y, D_004CB908, mark, label);
    return y + 12;
}

static inline int printLocatorHeaderRow(int y, int index,
    const char *const *label, double first_component,
    double third_component, double fourth_component)
{
    xglFontDebugPrintf(0, y, D_004CB890, index, first_component,
        third_component, fourth_component, *label);
    return y + 12;
}

static inline int printEnemyMotion(int y, int row, int slot,
    int motion, int motion_id)
{
    xglFontDebugPrintf(8, y, D_004CB9F0, row, slot, motion, motion_id);
    return y + 10;
}

static inline int printEnemyPointSummary(int y, int parameter,
    int spline_count, int point_count)
{
    xglFontDebugPrintf(8, y, D_004CB950, parameter, spline_count * 128, point_count);
    return y + 12;
}

static inline int printEnemySplineSummary(int y, int parameter, int count)
{
    return printEnemyPointSummary(y, parameter, count, count);
}

static inline int printEnemyHeading(int x, int y, const char *format)
{
    xglFontDebugPrintf(x, y, format);
    return y + 24;
}

static inline int printEnemySound(int y, int slot, int sound_effect)
{
    xglFontDebugPrintf(8, y, D_004CB8E8, slot, sound_effect);
    return y + 12;
}

static inline int printEnemySplinePoint(int y, int row,
    double position_x, double position_y, double position_z)
{
    xglFontDebugPrintf(8, y, D_004CB990, row, position_x, position_y, position_z);
    return y + 10;
}

static inline void printEnemyPageHeader(const EnemyWork *enemy)
{
    if (FLAG_FRAME_60 == 1)
        xglFontDebugPrintf(8, 0x18, D_004CB160);
    if (FLAG_FRAME_60 == 2)
        xglFontDebugPrintf(8, 0x18, D_004CB180);
    xglFontDebugPrintf(0, 0, D_004CB1A0, Page_0, DebugActor, enemy->entry_id);
}

static inline int wrappedEnemyDebugPage(int page, int increment)
{
    return (page + increment) % 15;
}

void PauseMenuPageEnemy(void)
{
    const Actor *selected = &actor[DebugActor];
    const EnemyWork *enemy = &enepc[selected->number];
    const EnemyParameters *parameters = &selected->parameters;
    int line_spacing;
    short y;
    short row;
    short column;
    unsigned short id;
    const char *actor_labels[17] = {
        D_004CAEF0, D_004CAF08, D_004CAF18, D_004CAF28,
        D_004CAF38, D_004DB868, D_004DB870, D_004DB878,
        D_004DB880, D_004CAF50, D_004DB888, D_004CAF60,
        D_004CAF70, D_004CAF80, D_004CAF90, D_004CAFA0, 0
    };
    const char *pause_labels[14] = {
        D_004DB890, D_004CAFF8, D_004DB898, D_004CB008,
        D_004CB018, D_004DB8A0, D_004DB8A8, D_004CB028,
        D_004DB8B0, D_004DB8B8, D_004DB8C0, D_004DB8C8,
        D_004DB8D0, D_004CB038
    };
    const char *locator_labels[14] = {
        D_004DB8D8, D_004DB8E0, D_004DB8E8, D_004DB8F0,
        D_004DB8F8, D_004DB900, D_004DB908, D_004DB910,
        0, 0, 0, 0, 0, 0
    };
    const char *direction_labels[4] = {
        D_004DB918, D_004DB920, D_004DB928, D_004DB930
    };
    const char *freeze_labels[4] = {
        D_004DB938, D_004CB0C8, D_004CB0E0, D_004CB0F0
    };
    const char *mark_labels[3] = {D_004DB890, D_004CB120, D_004CB130};
    const char *boolean_labels[2] = {D_004DB940, D_004DB948};
    const char *route_labels[2] = {
        D_004DB958, "\245\271\245\327\245\351\245\244\245\363"
    };
    EnemyVector4 final_target;

    if (PadData.half_2a & 0x1000)
        Page_0 = wrappedEnemyDebugPage(Page_0, 14);
    if (PadData.half_2a & 0x4000)
        Page_0 = wrappedEnemyDebugPage(Page_0, 1);
    line_spacing = 12;
    if (PadData.half_2a & 0x8000) {
        for (column = 63; column > 0; --column) {
            short actor_index = (DebugActor + column) % 64;
            if (actor[actor_index].in_use_id > 0) {
                DebugActor = actor_index;
                break;
            }
        }
    }
    if (PadData.half_2a & 0x2000) {
        for (column = 1; column < 64; ++column) {
            short actor_index = (DebugActor + column) % 64;
            if (actor[actor_index].in_use_id > 0) {
                DebugActor = actor_index;
                break;
            }
        }
    }
    if (PadData.half_2a & 0x0800) {
        FLAG_FRAME_60 = (FLAG_FRAME_60 + 1) % 3;
        Vibration_Set_Strong(0xff, 0, 0x3c);
    }
    printEnemyPageHeader(enemy);
    y = line_spacing;
    if (actor[DebugActor].kind == 1) {
        y = printEnemyHeading(0x60, y, D_004CB1C0);
    } else {
        switch (Page_0) {
        case 0: {
            const Vector4 *position = &selected->position;
            UnduParamInit(&UnduTemp);
            UnduTemp.header = UnduDataGetHeader(enemy->map_index, 0x8000);
            UnduCheck(position, 0, &UnduTemp);
            {
                const char *position_format = D_004CB1E0;
                xglFontDebugPrintf(0x60, y, D_004CB1D0);
                y += 24;
                {
                    double position_x = position->x;
                    xglFontDebugPrintf(8, y, position_format,
                        position_x, selected->position.y, selected->position.z);
                }
                y += 12;
                xglFontDebugPrintf(8, y, D_004CB1F8,
                    enemy->goback_position.x, enemy->goback_position.y,
                    enemy->goback_position.z);
            }
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB210,
                enemy->step_limit.x, enemy->step_limit.y, enemy->step_limit.z);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB228,
                enemy->stop_cause.x, enemy->stop_cause.y, enemy->stop_cause.z);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB240, selected->rotation.y, selected->target_angle);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB258, parameters->move_speed, parameters->route_step_length);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB278, parameters->sight_distance, parameters->sight_angle);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB298, parameters->chase_speed, parameters->hearing_distance);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB2B8,
                parameters->chase_distance, enemy->chase_frame_limit);
            {
                y += line_spacing;
                final_target = enemy->final_target;
                xglFontDebugPrintf(8, y, D_004CB2D8,
                    &final_target, selected->body_radius);
            }
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB2F8, enemy->territory_index, enemy->map_index);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB318, enemy->territory_index,
                Get_Attr(position, enemy->map_index, 0));
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB318, enemy->territory_index,
                Get_Attr(position, enemy->map_index, 0x10));
            y += line_spacing;
            {
                int territory_index = enemy->territory_index;
                y = printEnemyTerritoryID(y, territory_index,
                    EneTerritoryID[territory_index]);
            }
            id = selected->in_use_id;
            if ((unsigned int)id - 0x4000u < 0x806u)
                xglFontDebugPrintf(0xc0, 0xa4, D_004CB358);
            else
                xglFontDebugPrintf(0xc0, 0xa4, D_004DB968);
            break;
        }
        case 1:
            y = printEnemyHeading(0x60, y, D_004CB368);
            if (selected->number != 0) {
                int pause_kind = enemy->pause_kind;
                const char *pause_label = pause_labels[pause_kind];
                y = printEnemyPause(y, pause_kind, pause_label);
            }
            {
                int action = enemy->action;
                int hit_limit = enemy->hit_limit;
                int hit_count = enemy->hit_count;
                const char *action_label = actor_labels[action];
                y = printEnemyAction(y, action, action_label, hit_count, hit_limit);
            }
            {
                int freeze_state = enemy->freeze_state;
                y = printEnemyFreeze(y, freeze_state, freeze_labels[freeze_state]);
            }
            xglFontDebugPrintf(8, y, D_004CB3C8, enemy->stop_cause.x, enemy->stop_cause.y, enemy->stop_cause.z);
            y += line_spacing;
            y = printEnemyHiddenUnit(y, enemy->hidden_unit);
            xglFontDebugPrintf(8, y, D_004CB400);
            y += line_spacing;
            y = printEnemyCounters(y, D_004CB420,
                parameters->stiff_duration, parameters->pause_interval);
            y = printEnemyCounters(y, D_004CB440,
                parameters->threat_duration, parameters->threat_interval);
            y = printEnemyCounter(y, D_004CB460, enemy->turn_count);
            y = printEnemyCounter(y, D_004CB480, enemy->explosion_count);
            y = printEnemyCounter(y, D_004CB498, enemy->electric_count);
            y = printEnemyCounter(y, D_004CB4B0, enemy->fall_count);
            y = printEnemyLookAt(y, enemy, selected);
            xglFontDebugPrintf(8, y, D_004CB4F0, enemy->sight_position[0], enemy->sight_position[1], enemy->sight_position[2]);
            y += 12;
            break;
        case 2:
            y = printEnemyHeading(0x60, y, D_004CB508);
            {
                const char *locator_label = boolean_labels[
                    (unsigned short)parameters->locator_flags & 1];
                y = printEnemyFlag(y, D_004CB518, parameters->locator_flags & 1,
                    locator_label);
            }
            {
                int flags = parameters->locator_flags;
                y = printEnemyFlag(y, D_004CB538, (flags >> 1) & 1,
                    boolean_labels[(flags & 2) != 0]);
            }
            xglFontDebugPrintf(8, y, D_004CB558, (parameters->locator_flags >> 2) & 1,
                boolean_labels[(parameters->locator_flags & 4) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB578, (parameters->locator_flags >> 3) & 1,
                boolean_labels[(parameters->locator_flags & 8) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB598, (parameters->locator_flags >> 4) & 1,
                boolean_labels[(parameters->locator_flags & 0x10) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB5B8, (parameters->locator_flags >> 16) & 1,
                boolean_labels[(parameters->locator_flags & 0x10000) != 0]);
            y += line_spacing;
            y = printEnemyFlag(y, D_004CB5D8, (parameters->locator_flags >> 17) & 1,
                boolean_labels[(parameters->locator_flags & 0x20000) != 0]);
            xglFontDebugPrintf(8, y, D_004CB5F8, (parameters->locator_flags >> 18) & 1,
                boolean_labels[(parameters->locator_flags & 0x40000) != 0]);
            y += line_spacing;
            break;
        case 3:
            y = printEnemyHeading(0x60, y, D_004CB618);
            xglFontDebugPrintf(8, y, D_004CB628, enemy->tentacle_range);
            y += 12;
            y = printEnemyCounter(y, D_004CB640, enemy->tentacle_duration);
            printEnemyFinalCounters(y, D_004CB658, enemy->locator_start_frame, enemy->locator_end_frame);
            y += line_spacing;
            break;
        case 4:
            y = printEnemyHeading(0x60, y, D_004CB670);
            xglFontDebugPrintf(8, y, D_004CB690, enemy->scale.current);
            y += 12;
            y = printEnemyCounters(y, D_004CB6A8,
                enemy->scale.frame, enemy->scale.duration);
            {
                const char *scale_format = D_004CB6C8;
                xglFontDebugPrintf(8, y, scale_format,
                    enemy->scale.start, enemy->scale.target);
                y += 12;
                /* Leave a blank row after the scale range. */
                y += line_spacing;
            }
            printEnemyFinalCounters(y, D_004CB6E8, parameters->light_level, enemy->applied_light_level);
            y += line_spacing;
            break;
        case 5:
            y = printEnemyHeading(0x60, y, D_004CB708);
            xglFontDebugPrintf(8, y, D_004CB718, enemy->state_flags & 1,
                boolean_labels[(unsigned short)enemy->state_flags & 1]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB730, (enemy->state_flags >> 2) & 1, boolean_labels[(enemy->state_flags & 4) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB748, (enemy->state_flags >> 3) & 1, boolean_labels[(enemy->state_flags & 8) != 0]);
            y += line_spacing;
            {
                int state_flags = enemy->state_flags;
                const char *route_label = route_labels[(state_flags & 0x10) != 0];
                xglFontDebugPrintf(8, y, D_004CB760, (state_flags >> 4) & 1,
                    route_label);
            }
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB778, (enemy->state_flags >> 5) & 1, boolean_labels[(enemy->state_flags & 0x20) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB790, (enemy->state_flags >> 6) & 1, boolean_labels[(enemy->state_flags & 0x40) != 0]);
            y += line_spacing;
            xglFontDebugPrintf(8, y, D_004CB7A8, (enemy->state_flags >> 7) & 1, boolean_labels[(enemy->state_flags & 0x80) != 0]);
            y += line_spacing;
            {
                int freeze_flags = enemy->state_flags;
                const char *freeze_label = boolean_labels[(freeze_flags & 0x100) != 0];
                xglFontDebugPrintf(8, y, D_004CB7C0, (freeze_flags >> 8) & 1,
                    freeze_label, enemy->freeze_saved_speed);
            }
            y += 12;
            {
                int gravity_mode = parameters->gravity_mode;
                xglFontDebugPrintf(8, y, D_004CB7E0, gravity_mode,
                    boolean_labels[gravity_mode == 1]);
            }
            y += 12;
            {
                int turn_lock = enemy->turn_lock;
                xglFontDebugPrintf(8, y, D_004CB7F8, turn_lock,
                    boolean_labels[turn_lock]);
            }
            y += 12;
            xglFontDebugPrintf(8, y, D_004CB810,
                selected->motion_number, enemy->previous_motion);
            y += 12;
            {
                xglFontDebugPrintf(8, y, D_004CB838, enemy->target_number,
                    enemy->target_position.x, enemy->target_position.y,
                    enemy->target_position.z);
            }
            y += 12;
            {
                const unsigned int *actor_flags = &selected->flags;
                xglFontDebugPrintf(8, y, D_004CB858);
                {
                    int x = 0xd0;
                    for (column = 0; column < 16; ++column, x -= 8) {
                        xglFontDebugPrintf(x, y, "%1d",
                            (*actor_flags >> column) & 1);
                    }
                }
                y += line_spacing;
                {
                    const char *high_flag_label = boolean_labels[(*actor_flags & 0x80000000u) != 0];
                    xglFontDebugPrintf(8, y, D_004CB868, *actor_flags >> 31, high_flag_label);
                }
            }
            y += 12;
            /* Leave a blank row before the footer. */
            y += line_spacing;
            break;
        case 6: {
            y = printEnemyHeading(0x60, y, D_004CB880);
            for (column = 0; column < 16; ++column) {
                LayoutHeader *entry = UnduDataGetHeader(0x207, column);
                if (entry == 0)
                    break;
                {
                    int locator_type = Get_LocaterType(column / 2);
                    const char *const *label = &locator_labels[locator_type];
                    y = printLocatorHeaderRow(y, column, label, entry->components[0],
                        entry->components[2], entry->components[3]);
                }
            }
            break;
        }
        case 7: {
            y = printEnemyHeading(0x60, y, D_004CB8A8);
            {
                const GameLoopStateRecord *runtime = &GameLoopState;
                const float *values = runtime->debug_values;
                for (column = 0; column < 16; ++column) {
                    const char *direction = direction_labels[runtime->debug_directions[column]];
                    xglFontDebugPrintf(0, y, D_004CB8C0, column, direction,
                        values[column], boolean_labels[runtime->debug_enabled[column]]);
                    y += line_spacing;
                }
            }
            break;
        }
        case 8: {
            const short *sound_effect;
            y = printEnemyHeading(0x60, y, D_004CB8D8);
            sound_effect = parameters->sound_effects;
            for (column = 0; column < 8; ++column, ++sound_effect) {
                y = printEnemySound(y, column, *sound_effect);
            }
            break;
        }
        case 9: {
            int mark;
            y = printEnemyHeading(0x60, y, D_004CB8F8);
            mark = enemy->mark;
            y = printEnemyMark(y, mark, mark_labels[mark]);
            printEnemyFinalCounters(y, D_004CB920, enemy->mark_timer, 0x40);
            y += line_spacing;
            break;
        }
        case 10: {
            y = printEnemyHeading(0x60, y, D_004CB938);
            y = printEnemySplineSummary(y, enemy->spline_parameter,
                enemy->spline_point_count);
            y = printEnemyCounter(y, D_004CB978, enemy->fast_point_count);
            row = 0;
            if (enemy->spline_point_count > 0) {
                do {
                    const float *point_z = &enemy->spline_coordinates[row * 4 + 2];
                    y = printEnemySplinePoint(y, row,
                        point_z[-2], point_z[-1], *point_z);
                    ++row;
                } while (row < enemy->spline_point_count);
            }
            break;
        }
        case 11: {
            y = printEnemyHeading(0x60, y, D_004CB9A8);
            y = printEnemySplineSummary(y, enemy->spline_parameter,
                enemy->spline_point_count);
            xglFontDebugPrintf(8, y, D_004CB9C8, enemy->route_index,
                enemy->action_index * 128, enemy->spline_point_count);
            y += line_spacing;
            for (row = 0; row < enemy->spline_point_count; ++row) {
                for (column = 0; column < 4; ++column) {
                    int motion = enemy->motion_tables[0][row][column];
                    if (motion != -1) {
                        int motion_id = enemy->motion_tables[1][row][column];
                        y = printEnemyMotion(y, row, column, motion, motion_id);
                    }
                }
            }
            break;
        }
        case 12:
        case 13: {
            short count;
            if (Page_0 == 12) {
                y = printEnemyHeading(0x60, y, D_004CBA18);
                count = parameters->pause_interval;
            } else {
                y = printEnemyHeading(0x60, y, D_004CBA30);
                count = parameters->threat_interval;
            }
            {
                short page = Page_0 - 12;
                y = printEnemyCounters(y, D_004CB920, enemy->page_count[page], count);
                for (row = 0; row < count; ++row) {
                    int status = enemy->page_status[page][row];
                    const char *status_label = boolean_labels[status == 1];
                    xglFontDebugPrintf(8, y, D_004DB978, row, status_label);
                    y += line_spacing;
                }
            }
            break;
        }
        case 14: {
            y = printEnemyHeading(8, y, D_004CBA48);
            {
                short spline_count = enemy->spline_point_count;
                short spline_parameter = enemy->spline_parameter;
                short point_count = enemy->route_point_count;
                xglFontDebugPrintf(8, y, D_004CB950, spline_parameter,
                    spline_count * 128, point_count);
            }
            y += 12;
            for (row = 0; row < enemy->route_point_count; ++row) {
                    const float *point_z = &enemy->route_coordinates[row * 4 + 2];
                    xglFontDebugPrintf(8, y, D_004CBA78, row,
                        point_z[-2], *point_z);
                    if (enemy->route_metadata[64 + row] != -1)
                        xglFontDebugPrintf(0xb0, y, D_004DB980,
                            enemy->route_metadata[64 + row]);
                    else
                        xglFontDebugPrintf(0xb0, y, D_004DB988);
                    for (column = 0; column < enemy->route_metadata[row]; ++column) {
                        xglFontDebugPrintf(0xc0 + column * 0x10, y, "%2d",
                            enemy->route_values[row][column]);
                    }
                    y += 10;
            }
            {
                unsigned short route_buttons = PadData.half_2a;
                if (route_buttons & 1)
                    SelNum = (SelNum + enemy->route_point_count - 1) % enemy->route_point_count;
                if (route_buttons & 2)
                    SelNum = (SelNum + 1) % enemy->route_point_count;
                xglFontDebugPrintf(0, SelNum * 10 + 48, D_004DB998);
            }
            break;
        }
        }
    }
    xglFontDebugPrintf(8, y, D_004DB9A0);
}

void Debug_Mark(const Vector4 *position, int entry)
{
    Matrix4 place;
    Vector4 translation;
    Vector4 rotation;
    Matrix4 colorMatrix;
    Matrix4 normalMatrix;
    StudioLight *light;

    rotation.y = (float)Times * D_004D8160 * 0.015625f;
    translation.z = position->z;
    translation.x = position->x;
    translation.y = position->y + 1.0f;
    translation.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&translation.x);
    xglMatrixStackRotY(rotation.y);
    xglMatrixStackSave(place);

    xglStudioGetLight(&light);
    xglLightCalcMatrix(light);
    /* The SDK copies the two matrix blocks through its fixed scratch GPR. */
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        : : "r" (colorMatrix), "r" (&light->unmodeled_70[64])
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        : : "r" (normalMatrix), "r" (&light->unmodeled_70[0])
        : "$2", "memory");
    nmlModelSetLight(colorMatrix, normalMatrix);
    nmlModelSetPlace(place);
    nmlModelEntry(entry);
}









































































































































const char D_004DB998[2] = ">";

const char D_004DB9A0[3] = "\242\255";

short EventID = 0;
signed char FlagExEvent = 0;

/* Partial byte representation: original signedness and semantic type remain unknown. */
unsigned char FlagOnEvent = 0;
