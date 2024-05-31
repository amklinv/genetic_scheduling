#include "Genetic.hpp"
#include "Scheduler.hpp"
#include <iostream>
#include <QApplication>

int main(int argc, char* argv[]) {
  int ret_code;
  QApplication app(argc, argv);
  Kokkos::initialize(argc, argv);
  {
    // Read the themes from yaml
    std::cout << "Reading themes from yaml\n";
    Theme::read("../../data/SIAM-MDS24/codes.yaml");

    // Read the citations from yaml
    std::cout << "Reading citations from yaml\n";
    Speaker::read("../../data/SIAM-MDS24/citations.yaml");

    // Read the rooms from yaml
    std::cout << "Reading rooms from yaml\n";
    Rooms rooms("../../data/SIAM-MDS24/rooms.yaml");

    // Read the timeslots from yaml
    std::cout << "Reading timeslots from yaml\n";
    Timeslots tslots("../../data/SIAM-MDS24/timeslots.yaml");

    // Read the minisymposia from yaml
    std::cout << "Reading minisymposia from yaml\n";
    Minisymposia mini("../../data/SIAM-MDS24/minisymposia.yaml", rooms, tslots);
 
    // Run the genetic algorithm
    Scheduler s(mini);
    Genetic<Scheduler> g(s);
    Kokkos::Timer timer;
    timer.reset();
    auto best_schedule = g.run(10000, 2000, 0.01, 100);
    printf("Runtime: %lf seconds\n", timer.seconds());
    s.record("schedule.md", best_schedule);

    // Create a table to display the schedule
    Schedule sched(best_schedule, mini);

    ret_code = app.exec();
  }
  Kokkos::finalize();
  return ret_code;
}
