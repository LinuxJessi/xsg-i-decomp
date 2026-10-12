#include "common.h"

#include "enemy_1.h"

/* Per-actor work records are cleared before use. */

#define D_004D8044 0.3f

#define D_004D8048 1.9f

#define D_004D804C -0.02f

#define D_004D8050 3.1415927f

#define D_004D8054 3.1415927f

#define D_004D8058 0.3f

#define D_004D805C 1.9f

#define D_004D8060 -0.02f

#define D_004D8008 0.1f

#define D_004D800C 0.01f

#define D_004D8010 0.01f

#define D_004D8014 0.01f

#define D_004D8018 0.01f

EnemyWorkEntry enepc[16] = {{0}};

static unsigned short CoolDown_0 = 0;

static unsigned short CoolDown_1 = 0;

void *AdrsEnemyPreset = 0;

void *AdrsEnemySpline = 0;

void *AdrsEnemyExclamation = 0;

void *AdrsEnemyQuestion = 0;

void *AdrsEnemySphere = 0;

void *AdrsEnemySquare = 0;

/* Distance thresholds and retreat speeds used by the two look-back paths.
 * The original has no named objects for them: cc1 emits each use as an
 * anonymous .lit4 literal (0x004D8034..0x004D8040). */

#define D_004D8034 0.012f

#define D_004D8038 0.06666667f

#define D_004D803C 0.012f

#define D_004D8040 0.06666667f

/* Per-actor work records are cleared before use. */

/* Distance thresholds and retreat speeds used by the two look-back paths. */

int Get_EffectCode(int attr)
{
    int effect[15] = {
        0x8100, 0x8200, 0x8300, 0x8400, 0x8500, 0x8600, 0x8700, 0x8800,
        0x8900, 0x8a00, 0x8b00, 0x8c00, 0x8d00, 0x8e00, 0x8f00,
    };
    short i;

    attr &= 0x8f00;
    for (i = 0; i < 15; i++) {
        if (attr == effect[i]) {
            return i;
        }
    }
    return -1;
}

void Enemy_Init(EnemyActor *self)
{
    EnemyParams *params = &self->params;
    EnemyWorkEntry *work = &enepc[self->number];
    short i;
    short j;
    unsigned char kind;

    if (params->preset_index == -1) {
        work->initial_motion_id = 30;
        work->locator_start_frame = 5;
        work->locator_end_frame = 25;
        {
            float distance = params->locator_distance;

            work->pause_kind = params->move_kind;
            work->locator_distance = distance;
        }
        self->body_radius = params->body_radius;
        work->value_0c = params->value_108;
    } else {
        params->value_d8 = AdrsEnemy[params->preset_index].value_d8;
        params->route_step_length = AdrsEnemy[params->preset_index].route_step_length;
        params->pause_interval = AdrsEnemy[params->preset_index].pause_interval;
        params->chase_distance = AdrsEnemy[params->preset_index].chase_distance;
        params->value_dc = AdrsEnemy[params->preset_index].value_dc;
        params->value_c8 = AdrsEnemy[params->preset_index].value_c8;
        params->threat_interval = AdrsEnemy[params->preset_index].threat_interval;
        params->chase_speed = AdrsEnemy[params->preset_index].chase_speed;
        work->value_4c = AdrsEnemy[params->preset_index].value_2c;
        params->sight_angle = AdrsEnemy[params->preset_index].sight_angle;
        work->initial_motion_id = AdrsEnemy[params->preset_index].initial_motion_id;
        params->sight_distance = AdrsEnemy[params->preset_index].sight_distance;
        work->locator_start_frame = AdrsEnemy[params->preset_index].locator_start_frame;
        params->hearing_distance = AdrsEnemy[params->preset_index].hearing_distance;
        work->locator_end_frame = AdrsEnemy[params->preset_index].locator_end_frame;
        work->locator_distance = AdrsEnemy[params->preset_index].locator_distance;
        work->pause_kind = AdrsEnemy[params->preset_index].move_kind;
        self->body_radius = AdrsEnemy[params->preset_index].body_radius;
        work->value_0c = AdrsEnemy[params->preset_index].value_108;
        work->state_flags = AdrsEnemy[params->preset_index].state_flags;
        work->chase_frame_limit = AdrsEnemy[params->preset_index].chase_frame_limit;
        work->value_3858 = AdrsEnemy[params->preset_index].value_3858;
    }
    work->wall_attribute = params->wall_attribute;
    work->event_type = params->event_type;
    self->target_angle = self->angle;
    if (work->route_point_count == 0) {
        work->route_point_count = params->route_value_count / 3;
        for (i = 0; i < work->route_point_count; i++) {
            Vector4 *point = &work->route_points[i];
            float *dest = &point->z;

            dest[-2] = params->route_values[i * 3];
            dest[-1] = params->route_values[i * 3 + 1];
            dest[0] = params->route_values[i * 3 + 2];
        }
    }
    if (work->pause_kind == 9 || work->pause_kind == 4) {
        for (i = 0; i < work->route_point_count - 2; i++) {
            work->route_points[work->route_point_count + i].x =
                work->route_points[work->route_point_count - i - 2].x;
            *(&work->route_points[work->route_point_count + i].z) =
                *(&work->route_points[work->route_point_count - i - 2].z);
        }
        work->route_point_count = work->route_point_count * 2 - 2;
        if (work->pause_kind == 9)
            work->pause_kind = 8;
        if (work->pause_kind == 4)
            work->pause_kind = 3;
    }
    for (j = 0; j < work->route_point_count; j++) {
        for (i = 0; i < 4; i++) {
            work->action_motion_table[j][i] = -1;
            work->motion_id_table[j][i] = 0;
        }
    }
    if (work->homing_count == 0) {
        if (params->homing_value_count != 0) {
            work->homing_count = 0;
            for (i = 0; i < params->homing_value_count / 4; i++) {
                Homing_Add(self->number, params->homing_values[i].x,
                           params->homing_values[i].y,
                           params->homing_values[i].z,
                           (short)params->homing_values[i].w);
            }
        }
    }
    kind = work->pause_kind;
    if (kind != 8 && kind != 3 && kind != 12 && kind != 13)
        Set_Spline_By_Random(self);
    else
        Set_Spline_By_Route(self, 0);
    for (i = 0; i < 8; i++) {
        work->route_marks[1][i] = 0;
        work->route_marks[0][i] = 0;
    }
    work->route_cursor[0] = 0;
    work->route_cursor[1] = 0;
    if (params->pause_interval != 0)
        work->route_marks[0][Get_Rnd(0, params->pause_interval) % 8] = 1;
    if (params->threat_interval != 0)
        work->route_marks[1][Get_Rnd(0, params->threat_interval) % 8] = 1;
    if (self->sound_effect_id >= 0x4000 && self->sound_effect_id <= 0x4f22)
        work->map_index = 772;
    else
        work->map_index = 768;
    if (params->ex_motion_resource != -1) {
        ACT_initExMotion(self, 0,
                         GameResource[params->ex_motion_resource].motion);
    }
    Enemy_ActionReady(self, 16);
    work->hit_limit = 0;
    if (GameLoopState.player == self)
        CallMethod_I(D_004CAB00, 100);
    else
        CallMethod_I(D_004CAB00, work->event_type);
    UnduParamInit(&self->undulation);
    self->undulation.attr_mask = 2;
    UnduCheck(&self->position, 0, &self->undulation);
    self->undulation.attr_mask |= 0x1000;
}

void Enemy_Damage_Explosion(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count;
    short limit;
    int stiff;

    work->goback_x = 0.0f;
    work->goback_z = 0.0f;
    TM_Enemy_Move_Step(actor->number, &work->step_limit, work->goback_x);
    actor->target_angle = Get_Angle(&actor->position,
                                    &GameLoopState.player->position);

    count = work->hit_count;
    limit = work->hit_limit;
    work->hit_count = count + 1;
    if ((short)count >= limit) {
        actor->flags &= ~ACTOR_FLAG_AILMENT_ACTIVE;
        stiff = work->stiff_flag;
        if (stiff & ENEMY_STIFF_ACTIVE_BIT)
            Enemy_ActionReady(actor, 7);
        else
            Enemy_ActionReady(actor, 6);
    }
}

void Enemy_Damage_Electric(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int count = work->hit_count + 1;
    short limit = work->hit_limit;

    work->hit_count = count;
    if ((short)count >= limit) {
        u32 flags = actor->flags;

        work->state_40.alert_level = 0;
        actor->flags = flags & ~ACTOR_FLAG_AILMENT_ACTIVE;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 2);
    }
}

void Enemy_Damage_Seal(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int count = work->hit_count + 1;
    short limit = work->hit_limit;

    work->hit_count = count;
    if ((short)count >= limit) {
        u32 flags = actor->flags;

        work->state_40.alert_level = 0;
        actor->flags = flags & ~ACTOR_FLAG_AILMENT_ACTIVE;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 1);
    }
}

void Enemy_Stiff(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int timer = work->stiff_timer - 1;

    work->stiff_timer = timer;
    if ((short)timer <= 0) {
        work->stiff_flag &= ~ENEMY_STIFF_ACTIVE_BIT;
        work->stiff_timer = 0;
        actor->speed = work->stiff_saved_speed;
        actor->flags &= ~ACTOR_FLAG_STIFF_ACTIVE;
        sefDeleteEffectCf(work->stiff_effect);
        work->stiff_effect = 0;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 2);
    } else {
        actor->speed = 0.0f;
    }
}

void Enemy_Found(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;

    work->hit_count = count + 1;
    if ((short)count >= work->hit_limit)
        Enemy_ActionReady(actor, 6);

    if (actor->sound_effect_id != 0x2201) {
        EnemyActor *player = GameLoopState.player;
        actor->target_angle = Get_Angle(&actor->position, &player->position);
    }
}

void Enemy_Tentacle(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    short count;
    Vector4 point;
    Vector4 mark;
    float distance;
    float radius;
    int unit;
    short i;

    if (work->locator_start_frame <= (short)work->hit_count &&
        (short)work->hit_count <= work->locator_end_frame) {
        distance = Get_Distance(&self->position, &GameLoopState.player->position);
        radius = self->body_radius +
                 (work->locator_distance - self->body_radius) *
                     ((short)work->hit_count - work->locator_start_frame) /
                     (work->locator_end_frame - work->locator_start_frame);
        self->target_angle =
            Get_Angle(&self->position, &GameLoopState.player->position);

        if (FLAG_FRAME_60) {
            for (i = 0; i < 64; i++) {
                float angle = i * 6.2831855f * 0.015625f;

                mark.x = self->position.x + sinf(angle) * radius;
                mark.y = self->position.y + 1.0f;
                mark.z = self->position.z + cosf(angle) * radius;
                mark.w = 1.0f;
                Debug_Mark(&mark, AdrsEnemySphere);
            }
        }

        Get_Point_By_AngleLength(&self->position, &point, self->angle, radius);
        unit = CrossCheckMapUnit(&self->position, &point);
        if (unit != -1)
            SendBrokenSignal(&MapUnit[unit]);

        if (GameLoopState.player->encount_lock == 0 && distance < radius)
            Check_Encount(self, 0, 0);
        else
            GameLoopState.encount_reset = -1;
    }

    count = work->hit_count;
    work->hit_count++;
    if (count >= work->hit_limit)
        Enemy_ActionReady(self, 1);
}

void Enemy_Chase(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    EnemyParams *params = &self->params;
    EnemyVector4 start;
    Vector4 hit;
    Vector4 cross_point;
    int color[4];
    EnemyVector4 square;
    float distance;
    float speed;
    int crossed;

    distance = Get_Distance(&self->position, &work->escape_target.position);
    if (D_004D8008 <= distance && !(work->stiff_flag & 2)) {
        speed = params->chase_speed;
        Set_Motion(self, Get_DefaultMotion(self, 2), 9);
    } else {
        speed = params->chase_speed / 1.5f;
        Set_Motion(self, Get_DefaultMotion(self, 1), 9);
    }

    start = self->position_pair;
    Check_Undu(&self->position, &work->escape_target.position, work->map_index,
               &hit, 1, 0, 0);
    crossed = CrossPointUwamono(self->body_radius, &self->position,
                                &work->escape_target.position, &cross_point);
    if (__builtin_fabsf(hit.x - work->escape_target.position.x) < D_004D800C &&
        __builtin_fabsf(hit.z - work->escape_target.position.z) < D_004D800C &&
        crossed == 0) {
        work->last_player_position = work->escape_target.position_pair;
        xglFontDebugPrintf(8, 100, D_004DB7F8);
    } else if (crossed == 1) {
        work->last_player_position.vector = cross_point;
        xglFontDebugPrintf(8, 112, D_004DB800);
    } else {
        if (Check_MoveSquare(self->body_radius, &work->escape_target.position,
                             &self->position, &square.vector,
                             work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 112, D_004DB808);
        }
    }

    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
    color[3] = 128;
    TM_Enemy_Move_Step(self->number, &work->last_player_position.vector, speed);
    if ((work->state_flags & 0x20) != 0) {
        self->position.y += (work->escape_target.position.y - self->position.y) *
                            D_004D8010;
    }

    if (work->move_result == 0x60) {
        if (Check_MoveSquare(self->body_radius, &cross_point, &self->position,
                             &square.vector, work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 124, D_004DB800);
        } else {
            work->last_player_position.vector = cross_point;
            xglFontDebugPrintf(8, 124, D_004DB810);
        }
    }

    if (distance >= params->chase_distance || work->move_result == 0x58 ||
        ((short)work->chase_frame++ >=
             (work->chase_frame_limit < 0 ? -work->chase_frame_limit : work->chase_frame_limit) &&
         work->chase_frame_limit != 0) ||
        (Get_Distance(&self->position, &work->last_player_position.vector) <
             D_004D8014 &&
         work->move_result != 0x60 && work->move_result != 99)) {
        work->state_40.alert_level = 0;
        Homing_Search(self);
        Enemy_ActionReady(self, 2);
    } else if (Get_Distance(&self->position,
                            &work->last_player_position.vector) < D_004D8018) {
        if (Check_MoveSquare(self->body_radius, &work->escape_target.position,
                             &self->position, &square.vector,
                             work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 112, D_004DB818);
            TM_Enemy_Move_Step(self->number, &work->last_player_position.vector,
                               speed);
        }
        self->target_angle = Get_Angle(&self->position,
                                       &actor[work->target_number].position);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/enemy_1", Enemy_Route);

void Enemy_Escape(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    Vector4 point;
    float distance[3];
    short i;
    int length;
    int cooldown;
    short motion;

    if (work->spline_point_count <= 0)
        return;

    for (i = 0; i < 3; i++) {
        length = work->spline_point_count * 32;
        GetBSplineLoop((work->state_40.spline_parameter + length + i - 1) % length,
                       work->spline_point_count, 32, work->spline_points,
                       &point);
        distance[i] = Get_Distance(&work->escape_target.position, &point);
    }

    if (distance[0] < distance[1])
        i = distance[1] < distance[2] ? 2 : 1;
    else
        i = distance[0] < distance[2] ? 2 : 0;

    work->state_40.spline_parameter =
        (work->state_40.spline_parameter + work->spline_point_count * 32 + i - 1) %
        (work->spline_point_count * 32);
    GetBSplineLoop(work->state_40.spline_parameter, work->spline_point_count, 32,
                   work->spline_points, &point);

    if (i == 1) {
        actor->target_angle =
            Get_Angle(&actor->position, &work->escape_target.position);
        cooldown = CoolDown_0;
        CoolDown_0 = cooldown - 1;
    } else {
        CoolDown_0 = 16;
        actor->target_angle = Get_Angle(&actor->position, &point);
    }

    if (D_004D8034 <= Get_Distance(&actor->position, &point) ||
        (short)CoolDown_0 > 0) {
        motion = Get_DefaultMotion(actor, 2);
    } else {
        motion = Get_DefaultMotion(actor, 1);
    }
    Set_Motion(actor, motion, 9);

    actor->speed = D_004D8038;
    work->goback_x = point.x - actor->position.x;
    work->goback_z = point.z - actor->position.z;
    actor->position.x = point.x;
    actor->position.z = point.z;
}

void Enemy_Route_Chase(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    Vector4 point;
    float distance[3];
    short i;
    int length;
    int cooldown;
    short motion;

    if (work->spline_point_count <= 0)
        return;

    for (i = 0; i < 3; i++) {
        length = work->spline_point_count * 32;
        GetBSplineLoop((work->state_40.spline_parameter + length + i - 1) % length,
                       work->spline_point_count, 32, work->spline_points,
                       &point);
        distance[i] = Get_Distance(&work->escape_target.position, &point);
    }

    if (distance[0] < distance[1])
        i = distance[0] < distance[2] ? 0 : 2;
    else
        i = distance[1] < distance[2] ? 1 : 2;

    work->state_40.spline_parameter =
        (work->state_40.spline_parameter + work->spline_point_count * 32 + i - 1) %
        (work->spline_point_count * 32);
    GetBSplineLoop(work->state_40.spline_parameter, work->spline_point_count, 32,
                   work->spline_points, &point);

    if (i == 1) {
        self->target_angle =
            Get_Angle(&self->position, &work->escape_target.position);
        cooldown = CoolDown_1;
        CoolDown_1 = cooldown - 1;
    } else {
        CoolDown_1 = 16;
        self->target_angle = Get_Angle(&self->position, &point);
    }

    if (D_004D803C <= Get_Distance(&self->position, &point) ||
        (short)CoolDown_1 > 0) {
        motion = Get_DefaultMotion(self, 2);
    } else {
        motion = Get_DefaultMotion(self, 1);
    }
    Set_Motion(self, motion, 9);

    self->speed = D_004D8040;
    work->goback_x = point.x - self->position.x;
    work->goback_z = point.z - self->position.z;
    self->position.x = point.x;
    self->position.z = point.z;
}

void Enemy_Pause(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count;
    int limit;

    if (work->pause_kind == 5 || work->pause_kind == 6) {
        if (actor->flags & ACTOR_FLAG_FACE_PLAYER) {
            actor->target_angle =
                Get_Angle(&actor->position, &GameLoopState.player->position);
        }
    }

    count = work->hit_count;
    limit = work->hit_limit - 1;
    work->hit_count = count + 1;
    if ((short)count >= limit) {
        switch (work->pause_kind) {
        case 5:
        case 6:
            Enemy_ActionReady(actor, 1);
            break;
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
            Enemy_ActionReady(actor, 4);
            if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
                work->state_40.spline_parameter += 2;
            break;
        case 12:
            Enemy_ActionReady(actor, 14);
            break;
        case 13:
            Enemy_ActionReady(actor, 15);
            break;
        default: {
            unsigned short direction = xglSRand() & 1;

            switch (direction) {
            case 0:
                Enemy_ActionReady(actor, 4);
                break;
            case 1:
                Enemy_ActionReady(actor, 2);
                break;
            }
            break;
        }
        }
    }
}

void Enemy_Threat(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;
    int limit = work->hit_limit - 1;

    work->hit_count = count + 1;
    if ((short)count >= limit) {
        Enemy_ActionReady(actor, 4);
        if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
            work->state_40.alert_level += 2;
    }
}

void Enemy_Unique(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;
    int limit = work->hit_limit - 1;

    work->hit_count = count + 1;
    if ((short)count >= limit) {
        int step = work->unique_step + 1;

        work->unique_step = step;
        if (work->action_motion_table[work->unique_pattern][(short)step] != -1) {
            Enemy_ActionReady(actor, 9);
        } else {
            Enemy_ActionReady(actor, 4);
            if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
                work->state_40.spline_parameter += 2;
        }
    }
}

void ACT_updateEnemy(EnemyActor *self)
{
    Vector4 marker;
    int color[4];
    EnemyVector4 pillar;
    EnemyParams *params = &self->params;
    EnemyWorkEntry *work = &enepc[self->number];
    int effect_code;
    float angle;
    short i;
    int type;
    unsigned short id;

    color[2] = 255;
    color[3] = 128;
    marker.y = 2.0f;
    marker.w = 1.0f;
    color[0] = 0;
    color[1] = 0;
    marker.x = 0.0f;
    marker.z = 0.0f;
    if (self->number == DebugActor) {
        xglFontDebugPrintf(8, 52, D_004CAB48, self->number, (double)self->anim_time);
        if (self->number == DebugActor) {
            xglFontDebugPrintf(8, 62, D_004CAB60, self->number, (double)self->anim_start);
            if (self->number == DebugActor) {
                xglFontDebugPrintf(8, 72, D_004CAB78, self->number, (double)self->anim_final);
                if (self->number == DebugActor) {
                    xglFontDebugPrintf(8, 82, D_004CAB90, self->number, (double)self->speed);
                    if (self->number == DebugActor) {
                        xglFontDebugPrintf(8, 92, D_004CABA8, self->number, self->anim_number);
                        if (self->number == DebugActor)
                            xglFontDebugPrintf(8, 102, D_004CABC0, self->number, (double)self->anim_blend);
                    }
                }
            }
        }
    }
    if (FLAG_FRAME_60) {
        pillar = self->position_pair;
        pillar.vector.x = self->body_radius;
        pillar.vector.y = 0.0f;
        DispPillar(&self->position, &pillar, color);
    }
    work->move_result = 99;
    if (params->gravity_mode != 1) {
        if (Get_Height(&self->position, work->map_index, 16) != -1000.0f)
            self->saved_position = self->position_pair;
    }
    if (work->sense_block_timer > 0)
        work->sense_block_timer--;
    if (!(self->flags & ACTOR_FLAG_COLLIDABLE) || self->sound_effect_id <= 0 ||
        self == GameLoopState.player)
        return;

    if (work->applied_light_level != params->light_level) {
        if (params->light_level == 1000) {
            self->flags &= ~0x800;
        } else if (work->applied_light_level == 1000) {
            self->flags |= 0x800;
            self->light_flags |= 3;
            self->light_blend_frames = 72;
        }
        self->light_scale = (float)params->light_level / 1000.0f;
    }
    work->applied_light_level = params->light_level;
    GameIdLightSet(self, 0);

    if (self->sound_effect_id != 16646 && self->sound_effect_id != 20594) {
        effect_code = Get_EffectCode(Get_Attr(&self->position, 0, 0));
        if (work->action == 6 || work->action == 3 || work->action == 4 ||
            work->action == 8 || work->action == 10 || work->action == 14 ||
            work->action == 15 || EfTab_Code[effect_code] == 1) {
            if (effect_code != -1) {
                EnemyEffect *effect;

                if (EfTab_Intr[effect_code] != -1)
                    self->effect_interval++;
                if ((EfTab_Intr[effect_code] != -1 &&
                     self->effect_interval >= EfTab_Intr[effect_code]) ||
                    self->effect_interval == -1) {
                    effect = sefCreateEffectCf(EfTab_Code[effect_code], 0, 0);
                    self->effects[self->effect_next] = effect;
                    if (effect != 0) {
                        float size = params->body_radius * EfTab_Scal[effect_code];

                        effect->owner = self;
                        self->effect_interval = 0;
                        self->effect_next = (self->effect_next + 1) % 8;
                        effect->scale_z = size;
                        effect->scale_y = size;
                        effect->scale_x = size;
                    }
                }
            } else {
                for (i = 0; i < 8; i++) {
                    if (self->effects[i])
                        sefDeleteEffectCf(self->effects[i]);
                }
                self->effect_interval = -1;
            }
        }
    }

    if (work->target_number == 0)
        work->escape_target.position_pair = EnemyTarget;
    else
        Get_EnemyIDPos(&actor[work->target_number].position,
                       &work->escape_target.position);
    Start_Enemy_Command(self);
    if (work->freeze_state == 3) {
        ACT_updateSequence(self);
        return;
    }
    if (work->escape_target.scale_frame != -1 &&
        (unsigned char)work->freeze_state < 2) {
        short frame = work->escape_target.scale_frame;
        short count = work->escape_target.scale_frame_count;
        float scale;

        work->escape_target.scale =
            work->escape_target.scale_to -
            (work->escape_target.scale_to - work->escape_target.scale_from) *
                (float)(count * count - frame * frame) / (float)(count * count);
        ACT_setMotion(self, self->anim_number);
        work->escape_target.scale_frame++;
        if (work->escape_target.scale_frame >= work->escape_target.scale_frame_count)
            work->escape_target.scale_frame = -1;
        scale = work->escape_target.scale;
        self->scale.x = self->scale.y = self->scale.z = scale;
        self->scale.w = 1.0f;
    }
    if (self->lookat_timer == 2)
        Actor_LookAt(self);
    if (self->sound_effect_id == 8705)
        Check_Locater(self);

    if (work->freeze_state != 0) {
        if (work->freeze_state < 2)
            Script_Action(self);
        if (Get_Distance(&self->position, &GameLoopState.player->position) <=
                self->body_radius + GameLoopState.player->body_radius + D_004D8044 &&
            __builtin_fabsf(self->position.y - GameLoopState.player->position.y) <
                D_004D8048 &&
            GameLoopState.player->encount_lock == 0 &&
            enepc[GameLoopState.player->number].encount_lock == 0 &&
            FLAG_FRAME_60 == 0)
            Check_Encount(self, 0, 0);
        {
            float scale = work->escape_target.scale;

            self->scale.x = self->scale.y = self->scale.z = scale;
            self->scale.w = 1.0f;
        }
        ACT_updateMotion(self);
        if (work->freeze_state == 1 && params->gravity_mode == 0) {
            float height;

            self->undulation_header = UnduDataGetHeader(work->map_index, 0x8000);
            height = Get_Height(&self->position, work->map_index, 16);
            self->ground_height = height;
            work->fall_speed += D_004D804C;
            self->position.y += work->fall_speed;
            if (self->position.y < height) {
                self->position.y = height;
                work->fall_speed = 0.0f;
            }
        }
        if (work->mark != 0)
            Disp_EnemyMark(self);
        self->undulation_header = UnduDataGetHeader(0, 0x8000);
        return;
    }

    if (self->sound_effect_id != 8705)
        Check_Locater(self);
    switch (work->action) {
    case 0:
        Enemy_Init(self);
    case 1:
        Enemy_Pause(self);
        break;
    case 2:
        Enemy_Threat(self);
        break;
    case 4:
        Enemy_Route(self);
        break;
    case 5:
        Enemy_Found(self);
        break;
    case 6:
        Enemy_Chase(self);
        break;
    case 10:
        Enemy_Tentacle(self);
        break;
    case 7:
        Enemy_Stiff(self);
        break;
    case 9:
        Enemy_Unique(self);
        break;
    case 11:
        Enemy_Damage_Explosion(self);
        break;
    case 12:
        Enemy_Damage_Electric(self);
        break;
    case 13:
        Enemy_Damage_Seal(self);
        break;
    case 14:
        Enemy_Escape(self);
        break;
    case 15:
        Enemy_Route_Chase(self);
        break;
    }

    if (params->sight_distance != 0.0f) {
        if (!(self->flags & 8) && work->sense_block_timer <= 0 &&
            enepc[0].encount_lock == 0) {
            if (params->sight_angle < 0.0f) {
                if (self->sound_effect_id == 8705 || self->sound_effect_id == 16646) {
                    if (Check_InsideFan_Wooo(self->angle + D_004D8050,
                                             params->sight_distance,
                                             __builtin_fabsf(params->sight_angle),
                                             self->body_radius, &self->position,
                                             &work->escape_target.position,
                                             work->map_index) == 1)
                        Check_Discovery(self);
                } else if (Check_InsideFan(self->angle + D_004D8054,
                                           params->sight_distance,
                                           __builtin_fabsf(params->sight_angle),
                                           self->body_radius, &self->position,
                                           &work->escape_target.position,
                                           work->map_index) == 1) {
                    Check_Discovery(self);
                }
            } else {
                if (self->sound_effect_id == 8705 || self->sound_effect_id == 16646) {
                    if (Check_InsideFan_Wooo(self->angle, params->sight_distance,
                                             __builtin_fabsf(params->sight_angle),
                                             self->body_radius, &self->position,
                                             &work->escape_target.position,
                                             work->map_index) == 1)
                        Check_Discovery(self);
                } else if (Check_InsideFan(self->angle, params->sight_distance,
                                           __builtin_fabsf(params->sight_angle),
                                           self->body_radius, &self->position,
                                           &work->escape_target.position,
                                           work->map_index) == 1) {
                    Check_Discovery(self);
                }
            }
        }
    }
    if (FLAG_FRAME_60 && (PadData.pressed & 1)) {
        angle = self->target_angle;
        work->state_40.spline_parameter = 0;
        BSpline_Init(work->spline_points, &self->position, angle);
        Enemy_ActionReady(self, 2);
        Homing_Search(self);
    }

    if ((unsigned short)self->sound_effect_id - 0x4000 < 0xf23u) {
        if (self->flags & ACTOR_FLAG_AILMENT_ACTIVE) {
            if (self->flags & ACTOR_FLAG_STIFF_ACTIVE) {
                if (work->action != 11 && work->action != 7)
                    Enemy_ActionReady(self, 11);
            }
            if (self->flags & 0x02000000) {
                if (work->action != 12)
                    Enemy_ActionReady(self, 12);
            }
            if (self->flags & 0x08000000) {
                if (work->action != 13)
                    Enemy_ActionReady(self, 13);
            }
        }
    }
    id = self->sound_effect_id;
    type = RES_GetEnemySeType(id);
    if (type == 1) {
        if (self->anim_number == 1 || self->anim_number == 3)
            Sound_FootStep(self);
    } else {
        if (work->action != 4) {
            xglSoundEffectStopID(RES_GetEnemySeBank(self->sound_effect_id) + 2,
                                 self->number + 1);
        } else if (SCRIPT_getCfTime() >= 3) {
            xglSoundEffectPosID(RES_GetEnemySeBank(self->sound_effect_id) + 2,
                                &self->position, 1, self->number + 1);
        }
        if (work->action != 6) {
            xglSoundEffectStopID(RES_GetEnemySeBank(self->sound_effect_id) + 4,
                                 self->number + 1);
        } else {
            xglSoundEffectPosID(RES_GetEnemySeBank(self->sound_effect_id) + 4,
                                &self->position, 1, self->number + 1);
        }
    }
    if (work->effect_timer[0] > 0) {
        if (--work->effect_timer[0] <= 0) {
            work->effect_timer[0] = 0;
            work->stiff_flag &= ~2;
            sefDeleteEffectCf(work->stiff_effect);
            work->stiff_effect = 0;
            xglSoundEffectStopID(0x10008, 0);
        }
    }
    if (work->effect_timer[1] > 0) {
        if (--work->effect_timer[1] == 0) {
            work->stiff_flag &= ~4;
            xglSoundEffectStopID(0x1000b, 0);
        }
    }
    if (FLAG_FRAME_60) {
        if (PadData.held & 1)
            GameLoopState.player = &actor[NowTester];
        else
            GameLoopState.player = &actor[0];
    }
    if (Get_Distance(&self->position, &GameLoopState.player->position) <=
            self->body_radius + GameLoopState.player->body_radius + D_004D8058 &&
        __builtin_fabsf(self->position.y - GameLoopState.player->position.y) <
            D_004D805C &&
        GameLoopState.player->encount_lock == 0 &&
        enepc[GameLoopState.player->number].encount_lock == 0 &&
        FLAG_FRAME_60 == 0)
        Check_Encount(self, 0, 0);
    Script_Action(self);
    if (work->state_flags & 4)
        Move_BeltConveyer(self);
    if (work->turn_lock != 1 && work->action != 7) {
        if (work->stiff_flag & 2)
            self->angle = Get_Cursol_by_Reduce_Speed_Angle_Loop(self->angle, self->target_angle, 36.0f);
        else
            self->angle = Get_Cursol_by_Reduce_Speed_Angle_Loop(self->angle, self->target_angle, 6.0f);
    }
    {
        float scale = work->escape_target.scale;

        self->scale.x = self->scale.y = self->scale.z = scale;
        self->scale.w = 1.0f;
    }
    if (params->gravity_mode != 1) {
        if (Get_Height(&self->position, work->map_index, 16) == -1000.0f)
            self->position_pair = self->saved_position;
    }
    ACT_updateMotion(self);
    if (params->gravity_mode == 0) {
        float height = Get_Height(&self->position, work->map_index, 16);

        self->ground_height = height;
        work->fall_speed += D_004D8060;
        if (!(work->state_flags & ENEMY_AIRBORNE_FLAG))
            self->position.y += work->fall_speed;
        if (self->position.y < height) {
            self->position.y = height;
            work->fall_speed = 0.0f;
        }
    }
    if (work->mark != 0)
        Disp_EnemyMark(self);
}

INCLUDE_ASM("asm/main/nonmatchings/enemy_1", TM_Enemy_Move_Step);

void TM_Check_Move_at_GoBack(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];

    actor->position.x -= work->goback_x;
    actor->position.z -= work->goback_z;
}

int TM_Check_Move(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    float height = Get_Height(&self->position, work->map_index, 0x10);
    int attr = Get_Attr(&self->position, work->map_index, 0x10);
    EnemyVector4 start;
    EnemyVector4 end;
    Vector4 hit;
    signed char blocker;

    if (height == -1000.0f) {
        TM_Check_Move_at_GoBack(self);
        work->move_result = 0x40;
        return 0;
    }

    end = self->position_pair;
    start = self->position_pair;
    end.vector.y = height;
    start.vector.x -= work->goback_x;
    start.vector.z -= work->goback_z;
    if (!Check_Undu(&start.vector, &end.vector, work->map_index, &hit, 1, 0,
                    0x10)) {
        TM_Check_Move_at_GoBack(self);
        work->move_result = 0x40;
        return 0;
    }

    if (attr != EneTerritoryID[work->wall_attribute] && work->wall_attribute != 0 &&
        work->action != 8 && (int)self->flags >= 0) {
        if (work->state_flags & 8) {
            if (work->action != 6) {
                TM_Check_Move_at_GoBack(self);
                work->move_result = 0x50;
                return 0;
            }
        } else {
            TM_Check_Move_at_GoBack(self);
            work->move_result = 0x58;
            return 0;
        }
    }

    blocker = Check_Actor(&self->position, self->number);
    if (blocker != -1) {
        TM_Check_Move_at_GoBack(self);
        work->move_result = blocker;
        return 0;
    }

    if ((int)self->flags >= 0 &&
        HitCheckMapUnitPosSize(&self->position, self->body_radius) != -1) {
        if (CheckBrokenMapUnit(&MapUnit[HitCheckMapUnitPosSize(
                &self->position, self->body_radius)]) & 2) {
            SendBrokenSignal(&MapUnit[HitCheckMapUnitPosSize(
                &self->position, self->body_radius)]);
        }
        TM_Check_Move_at_GoBack(self);
        work->move_result = 0x60;
        return 0;
    }

    work->move_result = 99;
    return 1;
}

char TM_Check_Before_Eyes(EnemyActor *self, float length)
{
    EnemyWorkEntry *work = &enepc[self->number];
    Vector4 point;
    short i;

    Get_Point_By_AngleLength(&self->position, &point, self->angle, length);

    for (i = 0; i < 64; i++) {
        if (actor[i].sound_effect_id <= 0)
            continue;
        if (actor[i].kind == 1)
            continue;
        if (!(actor[i].flags & ACTOR_FLAG_COLLIDABLE))
            continue;
        if (self->number == i)
            continue;
        if (Get_Distance(&point, &actor[i].position) <=
            self->body_radius + actor[i].body_radius) {
            return i;
        }
    }

    if (Get_Height(&self->position, work->map_index, 0) == -1000.0f)
        return 64;
    return 99;
}

void Set_EnemyMark(EnemyActor *actor, short mark)
{
    EnemyWorkEntry *work = &enepc[actor->number];

    work->mark = mark;
    work->mark_timer = 0;
}

void Disp_EnemyMark(void)
{
    EnemyMarkSet mark[16];

    mark[15].none = 0;
    mark[15].exclamation = AdrsEnemyExclamation;
    mark[15].question = AdrsEnemyQuestion;
    mark[15].sphere = AdrsEnemySphere;
    mark[14] = mark[15];
}
