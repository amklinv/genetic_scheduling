import csv
import yaml
from scholarly import scholarly, ProxyGenerator, MaxTriesExceededException # Google scholar API

# Set up a ProxyGenerator object to use free proxies
# This needs to be done only once per session
pg = ProxyGenerator()
pg.FreeProxies()
scholarly.use_proxy(pg)

def scholar_lookup(name, email):
    if not '@' in email:
        return 0
    domain = email.split('@')[1]
    search_query = scholarly.search_author(name + ' ' + domain)
    try:
        result = next(search_query)
        author = scholarly.fill(result, sections=['publications'], sortby='year')
        total_citations = 0
        for publication in author['publications']:
            if 'pub_year' in publication['bib']:
                year = publication['bib']['pub_year']
            else:
                continue
            if int(year) < 2017:
                break
            citations = publication['num_citations']
            total_citations += citations
        print(name + ' ' + email, 'has', total_citations)
        return total_citations
    except StopIteration:
        print('Could not find', name + ' ' + email)
    except:
        return None
    return 0

minisymposia = {}
with open(r'data/SIAM-MDS24/minisymposia.csv',encoding='utf8') as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    line_count = 0
    # For each minisymposium...
    for row in csv_reader:
        # Skip the first two rows of the data
        if line_count > 1:
            # Get the topic codes, session number, minisymposia title, and talk titles
            session_no = row[4]

            # Skip the last row of the data
            if session_no == '':
                continue

            # Remove blank topic codes
            codes = [i for i in row[1:4] if i != '']
            title = row[5]
            sessions = row[7:11]

            talks = []
            for session in sessions:
                if session == '':
                    continue
                words = session.split(' - ')
                talks.append(words[2])

            s = int(session_no)
            minisymposia[s] = {}
            minisymposia[s]["title"] = title
            minisymposia[s]["talks"] = talks
            minisymposia[s]["class codes"] = list(map(int, codes))
            minisymposia[s]["organizers"] = []
            minisymposia[s]["speakers"] = []
            minisymposia[s]["emails"] = []
        line_count += 1

# Read the speaker/organizer data
try:
    with open('data/SIAM-MDS24/people.yaml', 'r') as f:
        people = yaml.load(f, Loader=yaml.SafeLoader)
except:
    people = {}

with open(r'data/SIAM-MDS24/people.csv',encoding='utf8') as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    line_count = 0
    # For each person...
    for row in csv_reader:
        # Skip the first three rows of the data
        if line_count > 2:
            # Get the name, email address, session number, and role
            last_name = row[0]
            first_name = row[1]
            email = row[2]
            role = row[3]
            session_no = row[4]

            name = first_name + " " + last_name
            s = int(session_no)
            if not s in minisymposia:
                continue
            if role == "ORG: Mini organizer":
                minisymposia[s]["organizers"].append(name)
            else:
                minisymposia[s]["speakers"].append(name)
            minisymposia[s]["emails"].append(email)

            # Record relevant participants
            if not email in people:
                people[email] = {}
                people[email]["aliases"] = []
                people[email]["name"] = name
            else:
                if name != people[email]["name"] and not name in people[email]["aliases"]:
                    people[email]["aliases"].append(name)

        line_count += 1

# Remove duplicates from email lists
for m in minisymposia:
    minisymposia[m]["emails"] = list(set(minisymposia[m]["emails"]))

# Write minisymposia data to file
with open(r'data/SIAM-MDS24/minisymposia.yaml', 'w') as file:
    yaml.dump(minisymposia, file)

# Look up citation counts for relevant participants
for email in people:
    name = people[email]["name"]
    if not "citations" in people[email] or people[email]["citations"] == None:
        people[email]["citations"] = scholar_lookup(name, email)
        if people[email]["citations"] == None:
            break

with open(r'data/SIAM-MDS24/people.yaml', 'w') as file:
    yaml.dump(people, file)