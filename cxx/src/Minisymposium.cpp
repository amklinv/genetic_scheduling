#include "Minisymposium.hpp"
#include <algorithm>

Minisymposium::Minisymposium(const std::string& title) :
  title_with_part_(title) { }

Minisymposium::Minisymposium(const std::string& title, 
                             unsigned tid, 
                             const std::string& organizer, 
                             const std::vector<std::string>& speakers,
                             double average_citation_count,
                             unsigned part) :
  title_with_part_(title),
  tid_(tid),
  average_citation_count_(average_citation_count),
  part_(part)

{
  // Strip the part from the name
  size_t found = title.find(" - Part");
  if(found != std::string::npos) {
    title_without_part_ = title.substr(0, found);
  }
  else {
    title_without_part_ = title;
  }

  // Add the participants
  if(!organizer.empty())
    participants_.insert(organizer);
  for(const auto& speaker : speakers)
    participants_.insert(speaker);
}

bool Minisymposium::shares_participant(const Minisymposium& m) const {
  std::vector<std::string> intersection(participants_.size());
  auto it = std::set_intersection(participants_.begin(), participants_.end(),
                                  m.participants_.begin(), m.participants_.end(),
                                  intersection.begin());
  return it != intersection.begin();
}

bool Minisymposium::comes_before(const Minisymposium& m) const {
  return title_without_part_ == m.title_without_part_ && part_ < m.part_;
}

bool Minisymposium::shares_theme(const Minisymposium& m) const {
  return tid_ == m.tid_;
}

KOKKOS_FUNCTION
bool Minisymposium::higher_priority(const Minisymposium& m) const {
  return room_priority_ < m.room_priority_;
}

KOKKOS_FUNCTION
unsigned Minisymposium::tid() const {
  return tid_;
}

KOKKOS_FUNCTION
unsigned Minisymposium::priority() const {
  return room_priority_;
}

const std::string& Minisymposium::short_title() const {
  return title_without_part_;
}

const std::string& Minisymposium::full_title() const {
  return title_with_part_;
}

double Minisymposium::average_citation_count() const {
  return average_citation_count_;
}

void Minisymposium::set_priority(unsigned priority) {
  room_priority_ = priority;
}