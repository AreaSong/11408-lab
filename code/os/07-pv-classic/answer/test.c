#include "exam.h"
#include "pc.h"
#include "phi.h"
#include "rw.h"
#include "smoke.h"
#include "test.h"
#include "threads.h"

static void test_pc(void) {
    Scene s;
    CHECK_EQ(scene_pc(1, 1, &s), 0);
    CHECK_EQ(s.produced, 1);
    CHECK_EQ(s.consumed, 1);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_pc(3, 5, &s), 0);
    CHECK_EQ(s.produced, 5);
    CHECK_EQ(s.consumed, 5);
    CHECK_EQ(s.deadlock, 0);
}

static void test_order(void) {
    Scene s;
    CHECK_EQ(scene_order("right", &s), 0);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(s.produced, 1);
    CHECK_EQ(s.consumed, 1);
    CHECK_EQ(scene_order("wrong", &s), 0);
    CHECK_EQ(s.deadlock, 1);
    CHECK_EQ(s.consumed, 0);
    CHECK_EQ(scene_order("wrong-full", &s), 0);
    CHECK_EQ(s.deadlock, 1);
    CHECK_EQ(scene_order("plant", &s), 0);
    CHECK_EQ(s.deadlock, 1);
}

static void test_rw(void) {
    Scene s;
    CHECK_EQ(scene_rw("reader-cut", &s), 0);
    CHECK_EQ(s.cut, 1);
    CHECK_EQ(s.max_read, 2);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_rw("writer-cut", &s), 0);
    CHECK_EQ(s.cut, 2);
    CHECK_EQ(s.max_read, 1);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_rw("readers", &s), 0);
    CHECK_EQ(s.cut, 1);
    CHECK_EQ(s.max_read, 2);
}

static void test_phi(void) {
    Scene s;
    CHECK_EQ(scene_phi("dead", 5, &s), 0);
    CHECK_EQ(s.deadlock, 1);
    CHECK_EQ(s.meals, 0);
    CHECK_EQ(s.mismatch, 0);
    CHECK_EQ(scene_phi("dead", 2, &s), 0);
    CHECK_EQ(s.deadlock, 1);
    CHECK_EQ(scene_phi("limit", 5, &s), 0);
    CHECK_EQ(s.meals, 5);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_phi("odd", 5, &s), 0);
    CHECK_EQ(s.meals, 5);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_phi("both", 5, &s), 0);
    CHECK_EQ(s.meals, 5);
    CHECK_EQ(s.deadlock, 0);
}

static void test_exam_model(void) {
    Scene s;
    int code[] = {1, 2, 3, 1};
    CHECK_EQ(scene_2019(5, 4, &s), 0);
    CHECK_EQ(s.bowl, 4);
    CHECK_EQ(s.meals, 5);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_2019(5, 6, &s), 0);
    CHECK_EQ(s.bowl, 4);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_2019(5, 1, &s), 0);
    CHECK_EQ(s.bowl, 1);
    CHECK_EQ(s.meals, 5);
    CHECK_EQ(scene_smoke(code, 4, &s), 0);
    CHECK_EQ(s.smoked, 4);
    CHECK_EQ(s.mismatch, 0);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_smoke_split(&s), 0);
    CHECK_EQ(s.deadlock, 1);
    CHECK_EQ(s.mismatch, 1);
    CHECK_EQ(scene_2022(&s), 0);
    CHECK_EQ(s.check, 1);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_2024(&s), 0);
    CHECK_EQ(s.check, 1);
    CHECK_EQ(scene_2025(4, &s), 0);
    CHECK_EQ(s.watered, 4);
    CHECK_EQ(s.max_open, 3);
    CHECK_EQ(s.deadlock, 0);
    CHECK_EQ(scene_2025(1, &s), 0);
    CHECK_EQ(s.watered, 1);
    CHECK_EQ(s.deadlock, 0);
    CHECK(s.max_open >= 1 && s.max_open <= 3);
}

static void test_threads(void) {
    ThreadPc pc;
    ThreadRw rw;
    ThreadPhi phi;
    ThreadSmoke smoke;
    ThreadPlant plant;
    ThreadBowl bowl;
    ThreadWrong wrong;
    CHECK_EQ(thread_pc(&pc), 0);
    CHECK_EQ(pc.produced, 20);
    CHECK_EQ(pc.consumed, 20);
    CHECK_EQ(pc.sum, 210);
    CHECK_EQ(pc.overflow, 0);
    CHECK_EQ(thread_rw(&rw), 0);
    CHECK_EQ(rw.reads, 6);
    CHECK_EQ(rw.writes, 4);
    CHECK_EQ(rw.overlap, 0);
    CHECK_EQ(thread_phi(THREAD_PHI_LIMIT, &phi), 0);
    CHECK_EQ(phi.eaten, 10);
    CHECK_EQ(phi.bad, 0);
    CHECK_EQ(thread_phi(THREAD_PHI_ODD, &phi), 0);
    CHECK_EQ(phi.eaten, 10);
    CHECK_EQ(phi.bad, 0);
    CHECK_EQ(thread_smoke(&smoke), 0);
    CHECK_EQ(smoke.smoked, 6);
    CHECK_EQ(smoke.glue, 2);
    CHECK_EQ(smoke.paper, 2);
    CHECK_EQ(smoke.tobacco, 2);
    CHECK_EQ(thread_plant(&plant), 0);
    CHECK_EQ(plant.watered, 5);
    CHECK_EQ(plant.bad, 0);
    CHECK(plant.max_open >= 1 && plant.max_open <= 3);
    CHECK_EQ(thread_bowl(&bowl), 0);
    CHECK_EQ(bowl.eaten, 5);
    CHECK_EQ(bowl.bad, 0);
    CHECK(bowl.max_users >= 1 && bowl.max_users <= 2);
    CHECK_EQ(thread_wrong(&wrong), 0);
    CHECK_EQ(wrong.empty_waiters, 1);
    CHECK_EQ(wrong.mutex_waiters, 1);
}

int main(void) {
    test_pc();
    test_order();
    test_rw();
    test_phi();
    test_exam_model();
    test_threads();
    return TEST_SUMMARY();
}
