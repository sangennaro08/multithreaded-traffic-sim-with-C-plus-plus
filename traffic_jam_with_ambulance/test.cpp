#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include "globals.hpp"
#include "Intersection.hpp"
#include "./vehicles/car.hpp"
#include "./vehicles/ambulance.hpp"

// ──────────────────────────────────────────────
// TEST: Intersection – stato iniziale
// ──────────────────────────────────────────────

TEST(IntersectionTest, DirezioneTutteLibere) {
    Intersection inter;
    for (auto& [dir, free] : inter.directions)
        EXPECT_TRUE(free) << "Direzione " << dir << " dovrebbe essere libera";
}

TEST(IntersectionTest, FreeSpotsIniziale) {
    Intersection inter;
    EXPECT_EQ(inter.free_spots, 3);
}

TEST(IntersectionTest, NessunVeicoloAlInizio) {
    Intersection inter;
    EXPECT_TRUE(inter.vehicles_in_intersection.empty());
}

TEST(IntersectionTest, QueuesVuoteAlInizio) {
    Intersection inter;
    for (auto& [dir, q] : inter.entry_queues)
        EXPECT_TRUE(q.empty()) << "Queue " << dir << " dovrebbe essere vuota";
}

TEST(IntersectionTest, ForwardNonHaQueue) {
    Intersection inter;
    EXPECT_EQ(inter.entry_queues.count("Forward"), 0);
}

TEST(IntersectionTest, QuattroDirezioni) {
    Intersection inter;
    EXPECT_EQ(inter.directions.size(), 4);
    EXPECT_TRUE(inter.directions.count("Right"));
    EXPECT_TRUE(inter.directions.count("Left"));
    EXPECT_TRUE(inter.directions.count("Back"));
    EXPECT_TRUE(inter.directions.count("Forward"));
}

// ──────────────────────────────────────────────
// TEST: Variabili globali – stato iniziale
// ──────────────────────────────────────────────

TEST(GlobalsTest, CarsCompletedIniziale) {
    cars_completed = 0;
    EXPECT_EQ(cars_completed.load(), 0);
}

TEST(GlobalsTest, AmbulancesActiveIniziale) {
    ambulances_active = 0;
    EXPECT_EQ(ambulances_active.load(), 0);
}

TEST(GlobalsTest, AmbulancesLaunchedIniziale) {
    ambulances_launched = 0;
    EXPECT_EQ(ambulances_launched.load(), 0);
}

TEST(GlobalsTest, MaxPerIntersection) {
    EXPECT_EQ(max_per_intersection, 3);
}

// ──────────────────────────────────────────────
// TEST: Car
// ──────────────────────────────────────────────

TEST(CarTest, NomeCorretto) {
    Car car("Car1", 0);
    EXPECT_EQ(car.name, "Car1");
}

TEST(CarTest, StartIntersectionCorretto) {
    Car car("Car1", 2);
    EXPECT_EQ(car.start_intersection, 2);
}

TEST(CarTest, QueuesEnteredIniziale) {
    Car car("Car1", 0);
    EXPECT_EQ(car.queues_entered.load(), 0);
}

TEST(CarTest, AddQueueIncrementa) {
    Car car("Car1", 0);
    car.add_queue();
    EXPECT_EQ(car.queues_entered.load(), 1);
    car.add_queue();
    EXPECT_EQ(car.queues_entered.load(), 2);
}

TEST(CarTest, CrossingTimeNelRange) {
    Car car("Car1", 0);
    EXPECT_GE(car.crossing_time, 1);
    EXPECT_LE(car.crossing_time, 25); // max 5*5
}

TEST(CarTest, ArrivalTimeNelRange) {
    Car car("Car1", 0);
    EXPECT_GE(car.arrival_time, 3);
    EXPECT_LE(car.arrival_time, 15); // max 5*3
}

// ──────────────────────────────────────────────
// TEST: Ambulance
// ──────────────────────────────────────────────

TEST(AmbulanceTest, NomeCorretto) {
    Ambulance amb(1);
    EXPECT_EQ(amb.name, "Ambulance1");
}

TEST(AmbulanceTest, EntryDirectionBack) {
    Ambulance amb(1);
    EXPECT_EQ(amb.entry_direction, "Back");
}

TEST(AmbulanceTest, SirenInizialmenteFalse) {
    Ambulance amb(1);
    EXPECT_FALSE(amb.siren);
}

TEST(AmbulanceTest, PercentageIniziale) {
    Ambulance amb(1);
    EXPECT_FLOAT_EQ(amb.percentage.load(), 0.0f);
}

TEST(AmbulanceTest, AddPercentageIncrementa) {
    Ambulance amb(1);
    amb.add_percentage();
    EXPECT_NEAR(amb.percentage.load(), 0.1f, 0.001f);
}

TEST(AmbulanceTest, ResetAmbulance) {
    Ambulance amb(1);
    amb.add_percentage();
    amb.add_percentage();
    amb.reset_ambulance();
    EXPECT_FLOAT_EQ(amb.percentage.load(), 0.0f);
}

TEST(AmbulanceTest, PatientsNelRange) {
    Ambulance amb(1);
    EXPECT_GE(amb.patients, 1u);
    EXPECT_LE(amb.patients, 5u);
}

TEST(AmbulanceTest, CrossingTimeNelRange) {
    Ambulance amb(1);
    EXPECT_GE(amb.crossing_time, 3);  // min 1+2
    EXPECT_LE(amb.crossing_time, 7);  // max 5+2
}

TEST(AmbulanceTest, ArrivalTimeNelRange) {
    Ambulance amb(1);
    EXPECT_GE(amb.arrival_time, 3);   // min 1+2
    EXPECT_LE(amb.arrival_time, 7);   // max 5+2
}

// ──────────────────────────────────────────────
// MAIN
// ──────────────────────────────────────────────

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}