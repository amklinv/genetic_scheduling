#ifndef MINISYMPOSIA_H
#define MINISYMPOSIA_H

#include "Minisymposium.hpp"
#include "Rooms.hpp"
#include "Theme.hpp"
#include "Timeslots.hpp"
#include <Kokkos_Core.hpp>
#include <ostream>
#include <set>
#include <vector>

class Minisymposia {
public:
  Minisymposia(const std::string& filename);
  Minisymposia(const std::string& filename, const Rooms& rooms, const Timeslots& slots);
  Minisymposia(const Minisymposia&) = default;
  ~Minisymposia() = default;
  Minisymposia& operator=(const Minisymposia&) = delete;

  unsigned find(unsigned mid) const;
  
  KOKKOS_FUNCTION unsigned size() const;
  KOKKOS_FUNCTION const Minisymposium& operator[](unsigned i) const;
  const Minisymposium& get(unsigned i) const;

  KOKKOS_FUNCTION bool overlaps_participants(unsigned m1, unsigned m2) const;
  KOKKOS_FUNCTION bool breaks_ordering(unsigned m1, unsigned m2) const;
  unsigned get_max_penalty() const;
  void set_room_penalties(const Rooms& rooms);
  void set_overlapping_participants();
  void set_prerequisites();
  void set_valid_timeslots(const Timeslots& slots);
  KOKKOS_FUNCTION double map_priority_penalty(unsigned nproblems) const;
  void set_priorities(unsigned nslots);
  void set_priority_penalty_bounds(unsigned nslots);
  void set_overlapping_themes(unsigned nrooms, unsigned nslots);
  KOKKOS_FUNCTION const Theme& class_codes(unsigned mid, unsigned cid) const;
  Kokkos::View<Theme*[3]>::HostMirror class_codes() const;

  KOKKOS_FUNCTION const Timeslots& timeslots() const;
  KOKKOS_FUNCTION const Rooms& rooms() const;

  template<class ViewType>
  KOKKOS_INLINE_FUNCTION double rate_schedule(ViewType schedule, 
    unsigned& order_penalty, unsigned& gumband_time_penalty, unsigned& gumband_room_penalty,
    unsigned& oversubscribed_penalty, double& theme_penalty, unsigned& timeslot_penalty,
    unsigned& room_penalty, unsigned& priority_penalty) const;
  
  template<class ViewType>
  inline std::string rate_schedule(ViewType schedule) const;

  friend std::ostream& operator<<(std::ostream& os, const Minisymposia& mini);
private:
  Kokkos::View<Theme*[3]> class_codes_;
  Kokkos::View<Minisymposium*> d_data_;
  Kokkos::View<Minisymposium*>::HostMirror h_data_;
  Kokkos::View<bool**> same_participants_;
  Kokkos::View<bool**> is_prereq_;
  Kokkos::View<double**> theme_penalties_;
  Kokkos::View<bool**> valid_timeslots_;
  Rooms rooms_;
  Timeslots timeslots_;
  unsigned nprereqs_;
  unsigned max_penalty_{4};
  unsigned min_priority_penalty_{0};
  unsigned max_priority_penalty_{0};
};

template<class ViewType>
KOKKOS_INLINE_FUNCTION 
double Minisymposia::rate_schedule(ViewType schedule, 
    unsigned& order_penalty, unsigned& gumband_time_penalty, unsigned& gumband_room_penalty,
    unsigned& oversubscribed_penalty, double& theme_penalty, unsigned& timeslot_penalty,
    unsigned& room_penalty, unsigned& priority_penalty) const
{
  unsigned nrooms = schedule.extent(1);
  unsigned nslots = schedule.extent(0);
  unsigned nmini = size();
  // Compute the penalty related to multi-part minisymposia being out of order
  order_penalty = 0;
  for(unsigned sl1=0; sl1<nslots; sl1++) {
    for(unsigned r1=0; r1<nrooms; r1++) {
      if(schedule(sl1,r1) >= nmini) continue;
      for(unsigned sl2=sl1; sl2<nslots; sl2++) {
        for(unsigned r2=0; r2<nrooms; r2++) {
          if(schedule(sl2,r2) >= nmini) continue;
          if(breaks_ordering(schedule(sl1,r1), schedule(sl2,r2))) {
            order_penalty++;
          }
        }
      }
    }
  }
  // Compute the penalty related to multi-part minisymposia being too far away from each other
  gumband_time_penalty = nprereqs_;
  for(unsigned sl=0; sl<nslots-1; sl++) {
    for(unsigned r1=0; r1<nrooms; r1++) {
      unsigned m1 = schedule(sl,r1);
      if(m1 >= nmini) continue;
      for(unsigned r2=0; r2<nrooms; r2++) {
        unsigned m2 = schedule(sl+1,r2);
        if(m2 >= nmini) continue;
        if(is_prereq_(m1, m2)) {
          gumband_time_penalty--;
        }
      }
    }
  }
  // Compute the penalty related to multi-part minisymposia being in different rooms
  gumband_room_penalty = nprereqs_;
  for(unsigned sl1=0; sl1<nslots; sl1++) {
    for(unsigned r=0; r<nrooms; r++) {
      unsigned m1 = schedule(sl1, r);
      if(m1 >= nmini) continue;
      for(unsigned sl2=0; sl2<nslots; sl2++) {
        unsigned m2 = schedule(sl2, r);
        if(m2 >= nmini) continue;
        if(is_prereq_(m1,m2)) {
          gumband_room_penalty--;
        }
      }
    }
  }
  // Compute the penalty related to oversubscribed participants
  oversubscribed_penalty = 0;
  for(unsigned sl=0; sl<nslots; sl++) {
    for(unsigned r1=0; r1<nrooms; r1++) {
      if(schedule(sl,r1) >= nmini) continue;
      for(unsigned r2=r1+1; r2<nrooms; r2++) {
        if(schedule(sl,r2) >= nmini) continue;
        if(overlaps_participants(schedule(sl,r1), schedule(sl,r2))) {
          printf("%i and %i share a participant in timeslot %i\n", 
                 d_data_(schedule(sl,r1)).id(), d_data_(schedule(sl,r2)).id(), sl+1);
          oversubscribed_penalty++;
        }
      }
    }
  }
  // Compute the penalty related to theme overlap
  theme_penalty = 0;
  for(unsigned sl=0; sl<nslots; sl++) {
    for(unsigned r=0; r<nrooms; r++) {
      unsigned mini_index = schedule(sl,r);
      if(mini_index >= nmini) continue;
      for(unsigned r2=r+1; r2<nrooms; r2++) {
        unsigned mini_index2 = schedule(sl,r2);
        if(mini_index2 >= nmini) continue;
        theme_penalty += theme_penalties_(r,r2);
      }
    }
  }

  // Compute the penalty related to scheduling speakers at a time they're not available
  timeslot_penalty = 0;
  for(unsigned sl=0; sl<nslots; sl++) {
    for(unsigned r=0; r<nrooms; r++) {
      unsigned mini_index = schedule(sl,r);
      if(mini_index >= nmini) continue;
      if(!valid_timeslots_(mini_index, sl)) {
        printf("%i is in invalid timeslot %i\n", d_data_(mini_index).id(), sl+1);
        timeslot_penalty++;
      }
    }
  }

  // Compute the penalty related to priority and room requests
  room_penalty = 0;
  priority_penalty = 0;
  for(unsigned sl=0; sl<nslots; sl++) {
    for(unsigned r=0; r<nrooms; r++) {
      unsigned mini_index = schedule(sl,r);
      if(mini_index >= nmini) continue;
      unsigned room_id = d_data_[mini_index].room_id();
      // This is a minisymposium with a room request
      if(room_id < nrooms) {
        if(room_id != r) {
          printf("%i is in invalid room %i\n", d_data_(mini_index).id(), r);
          room_penalty++;
        }
      }
      // There is no room request
      else {
        unsigned priority = d_data_[mini_index].priority();
        if(priority < r) {
          priority_penalty += pow(r-priority, 2);
        }
      }
    }
  }
  double penalty = order_penalty + oversubscribed_penalty + room_penalty + timeslot_penalty;
  penalty += theme_penalty + (gumband_time_penalty + gumband_room_penalty)/nprereqs_ + map_priority_penalty(priority_penalty);
  return 1 - penalty / max_penalty_;
}

template<class ViewType>
inline std::string Minisymposia::rate_schedule(ViewType schedule) const {
  Kokkos::View<unsigned*> d_penalties("penalties", 7);
  Kokkos::View<double*> d_penalty("penalty", 1);
  double score;
  Kokkos::parallel_reduce("computing score", 1, KOKKOS_CLASS_LAMBDA (unsigned i, double& lscore) {
    lscore = rate_schedule(schedule, d_penalties(0), d_penalties(1), d_penalties(2),
      d_penalties(3), d_penalty(0), d_penalties(4), d_penalties(5), d_penalties(6));
  }, score);

  // Copy penalties back to device
  auto h_penalties = Kokkos::create_mirror_view(d_penalties);
  auto h_penalty = Kokkos::create_mirror_view(d_penalty);
  Kokkos::deep_copy(h_penalties, d_penalties);
  Kokkos::deep_copy(h_penalty, d_penalty);

  std::ostringstream oss;

  oss << "This schedule has score " << score 
      << "\nThe penalty for multipart minisymposia being out of order is " << h_penalties(0)
      << "\nThe penalty for multipart minisymposia not being in consecutive timeslots is " << h_penalties(1)
      << "\nThe penalty for multipart minisymposia not being in the same room is " << h_penalties(2)
      << "\nThe penalty for putting the same participant in two rooms at once is " << h_penalties(3)
      << "\nThe penalty for putting too many minisymposia with the same themes in the same timeslot is " << h_penalty(0) 
      << "\nThe penalty for putting a speaker in a timeslot they're not available is " << h_penalties(4)
      << "\nThe penalty for putting a minisymposium in a room other than what they requested is " << h_penalties(5)
      << "\nThe penalty for giving a minisymposium too small a room based on estimated popularity is " << h_penalties(2) 
      << " in [ " << min_priority_penalty_ << ", " << max_priority_penalty_ << "]";
  return oss.str();
}

#endif /* MINISYMPOSIA_H */