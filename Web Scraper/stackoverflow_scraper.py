from flask import Flask, jsonify, request, Response
import os
import requests
from bs4 import BeautifulSoup
import re
import pytz
import json
import time
from datetime import datetime
from collections import OrderedDict

app = Flask(__name__)

GMT_zone = pytz.timezone('UTC')


def make_request(url):
    
    delay = 60

    while True:
        response = requests.get(url)
        
        if not response.status_code == 429:
            return response
        
        time.sleep(delay)
        delay *= 2  
    


@app.route('/questions', methods=['GET'])
def get_questions():

    site = request.args.get('site')
    if not site or site.lower() != 'stackoverflow':
        return jsonify({"error": "The 'site' parameter is required and must be 'stackoverflow'"}), 400


    min_value = request.args.get('min', type=int)
    max_value = request.args.get('max', type=int)
    sort = request.args.get('sort', default='activity')
    order = request.args.get('order', default='desc')
    filter = request.args.get('filter', default='default')
    tagged = request.args.get('tagged', default='', type=str)
    page = request.args.get('page', default=1, type=int)
    pagesize = request.args.get('pagesize', default=3, type=int)


    valid_sorts = ['activity', 'creation', 'votes', 'hot', 'week', 'month']
    if sort not in valid_sorts:
        return jsonify({"error": f"Invalid sort parameter. Valid options are: {', '.join(valid_sorts)}"}), 400

    valid_orders = ['desc', 'asc']
    if order not in valid_orders:
        return jsonify({"error": f"Invalid order parameter. Valid options are: {', '.join(valid_orders)}"}), 400

    if page < 1:
        return jsonify({"error": "Page number must be 1 or greater"}), 400
    

    base_url = "https://stackoverflow.com/questions"
    url = base_url

    query_params = []

    tags_to_filter = [tag.strip() for tag in tagged.split(';') if tag.strip()]
    if len(tags_to_filter) > 3:
        return jsonify({"error": "You can specify up to 3 tags only."}), 400
    if tags_to_filter:
        tags_query = '+'.join(f'[{tag}]' for tag in tags_to_filter)
        query_params.append(tags_query)

        query_string = '+'.join(query_params)
        url = f"{base_url}/tagged/{query_string}"


    if sort in ['hot', 'week', 'month']:
        if '?' in url:
            url += f"&tab={sort}"
        else:
            url += f"?tab={sort}"

    if '?' in url:
        url += f"&page={page}"
    else:
        url += f"?page={page}"

    print(url)

 
    withbody = False
    total = False

    if filter not in ['default', 'withbody', 'none', 'total']:
        return jsonify({"error": "Invalid filter type"}), 400
    if filter == "none":
        return jsonify({})
    if filter == "withbody":
        withbody = True
    if filter == "total":
        total = True


    response = make_request(url)

    if response.status_code != 200:
        return jsonify({"error": "Failed to fetch questions from Stack Overflow"}), 500

    items = []

    soup = BeautifulSoup(response.content, "html.parser")

    if total:
        total_div = soup.find('div', class_='fs-body3 flex--item fl1 mr12 sm:mr0 sm:mb12')
        questions_text = total_div.get_text(strip=True)
        questions_text = questions_text.replace('\n', '').replace('\r', '').strip()
        number_of_questions = questions_text.split(' ')[0].replace(',', '')
        if pagesize < int(number_of_questions):
            return jsonify({"Total": pagesize})
        return jsonify({"Total": number_of_questions})

    count = 0

    question_summaries = soup.find_all("div", class_="s-post-summary")
    for summary in question_summaries:
        count += 1
        if count > pagesize: break

        question_id = int(summary["data-post-id"])

        url = f"https://stackoverflow.com/questions/{question_id}"

        response = make_request(url)

        if response.status_code != 200:
            return jsonify({"error": "Failed to fetch the question from Stack Overflow"}), 500

        soup = BeautifulSoup(response.content, "html.parser")

        aside_element = soup.find('aside', class_='js-bounty-notification')

        bounty_amount = None
        bounty_closes_date = None

        if aside_element:
            date_span = aside_element.find('span', title=True)
            date_bounty_str = date_span['title'] if date_span else None

            date_bounty_norm = datetime.strptime(date_bounty_str, "%Y-%m-%d %H:%M:%SZ")
            GMT_creation = GMT_zone.localize(date_bounty_norm)
            zone = GMT_creation.astimezone(pytz.UTC)
            bounty_closes_date = int(zone.timestamp())

            bounty_span = aside_element.find('span', class_='s-badge__bounty')
            bounty_amount = int(bounty_span.text.strip().replace('+', '')) if bounty_span else None

        summary = soup.find("div", class_="question")

        if summary:

            score = 0

            div_tag = summary.find('div', class_='js-voting-container')
            quest_id = int(div_tag['data-post-id']) if div_tag else None

            div_tag = summary.find('div', class_='js-vote-count')
            score = int(div_tag['data-value']) if div_tag else None

            user_card = summary.find("div", class_="user-details", itemprop="author") 
            profile_link = "aa"
            display_name = ""

            anchor_tag = user_card.find("a") if user_card else None

            if anchor_tag and anchor_tag.get("href"):
                profile_link = "https://stackoverflow.com" + anchor_tag["href"]
                display_name = anchor_tag.text.strip() 
                            

            last_edited_date = None

            edited_element = summary.find('a', title="show all edits to this post")
            if edited_element:
                last_edited_date_str = edited_element.find('span', class_='relativetime')['title']
                last_edited_date_norm = datetime.strptime(last_edited_date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_last_edited = GMT_zone.localize(last_edited_date_norm)
                last_edited_date = int(GMT_last_edited.astimezone(pytz.UTC).timestamp())


            modified_date_element = soup.find('a', href="?lastactivity")
            modified_date = modified_date_element['title'] if modified_date_element else None

            if modified_date:
                modified_date_norm = datetime.strptime(modified_date, "%Y-%m-%d %H:%M:%SZ")
                GMT_last = GMT_zone.localize(modified_date_norm)
                zone = GMT_last.astimezone(pytz.UTC)
                last_activity_date = int(zone.timestamp())

            
            tag_elements = summary.find_all("a", class_="post-tag")
            tags = [tag.text.strip() for tag in tag_elements]

           
            view_tag = soup.find('div', class_='flex--item ws-nowrap mb8')

        
            if view_tag and 'title' in view_tag.attrs:
                title = view_tag['title']
                view_str = title.split()[1].replace(',', '')
                view_count = int(view_str)
            else:
                view_count = None

            answer_count = len(soup.find_all("div", class_="answer"))

            if answer_count > 0:
                is_answered = True
            else:
                is_answered = False

            title = soup.find("a", class_="question-hyperlink").text.strip()

            link = url
            
            time_url = "https://stackoverflow.com/posts/" + str(question_id) + "/timeline"

            response_date = make_request(time_url)
            if response_date.status_code == 200:
                time_soup = BeautifulSoup(response_date.content, 'html.parser')   
                
           
                closed_date = None
                closed_reason = None
                protected_date = None
                locked_date = None
                community_owned_date = None
                unprotected = True

                migrated_date = None
                migrated_id = None
                migrated_link = None
        
                for row in time_soup.find_all('tr'):
                    
                    date_element = row.find('span', class_='relativetime')
                    date_str = date_element['title'] if date_element else None

                    if not date_str:
                        continue


                    date_norm = datetime.strptime(date_str, "%Y-%m-%d %H:%M:%SZ")
                    GMT_creation = GMT_zone.localize(date_norm)
                    zone = GMT_creation.astimezone(pytz.UTC)
                    date = int(zone.timestamp())

                    event_cell = row.find('td', class_='wmn1')
                    if event_cell:
                        event_text = event_cell.text.strip().lower()

                
                        if "migrated" == event_text:
                            migrated_date = date

                            comment_cell = row.find('td', class_='event-comment')
                            if comment_cell:
                                link_tag = comment_cell.find('a')
                                if link_tag and 'href' in link_tag.attrs:
                                    migrated_link = link_tag['href']
                                    match = re.search(r'/questions/(\d+)/', migrated_link)
                                    if match:
                                        migrated_id = int(match.group(1))
                                    else:
                                        migrated_id = None 


                        if 'unprotected' == event_text:
                            unprotected = False

                        if 'closed' == event_text and not closed_date:
                            closed_date = date
                            comment_element = row.find('td', class_='event-comment')
                            closed_reason = comment_element.text.strip() if comment_element else None

                        if unprotected:
                            if 'protected' == event_text and not protected_date:
                                protected_date = date

                        if 'locked' in event_text and not locked_date:
                            locked_date = date

                        if "made wiki" == event_text:
                            community_owned_date = date
    
                user_not_exist = False
                display_name = ""
                profile_elements = time_soup.find_all('a', class_='comment-user owner')

               
                if profile_elements:
                  
                    last_profile_element = profile_elements[-1]
                    display_name = last_profile_element.text.strip()

                    if profile_link == "aa":
                        profile_link = "https://stackoverflow.com" + last_profile_element['href']

            
                    acc = make_request(profile_link)

                    if response.status_code == 200:
                        acc_soup = BeautifulSoup(acc.content, "html.parser")

                        img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                        profile_image = img_tag['src'] if img_tag else None

                        user_id = profile_link.split("/")[-2]
                        user_type = "registered"

                        reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                        if "m" in reputation_text:
                            reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                        elif "k" in reputation_text:
                            reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                        else:
                            reputation = int(reputation_text.replace(',', ''))

                        reputation = int(reputation_text.replace(',', ''))

                        script_tags = acc_soup.find_all("script")
                        account_id = None

                        for script in script_tags:
                            script_text = script.string
                            if script_text and "StackExchange.user.init" in script_text:
                    
                                account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                                if account_id_match:
                                    account_id = account_id_match.group(1)
                                    break

                elif profile_link != "aa":

                    acc = make_request(profile_link)

                    if response.status_code == 200:
                        acc_soup = BeautifulSoup(acc.content, "html.parser")

                        img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                        profile_image = img_tag['src'] if img_tag else None

                        user_id = profile_link.split("/")[-2]
                        user_type = "registered"

                        reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                        if "m" in reputation_text:
                            reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                        elif "k" in reputation_text:
                            reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                        else:
                            reputation = int(reputation_text.replace(',', ''))

                        reputation = int(reputation_text.replace(',', ''))

                        script_tags = acc_soup.find_all("script")
                        account_id = None

                        for script in script_tags:
                            script_text = script.string
                            if script_text and "StackExchange.user.init" in script_text:
                    
                                account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                                if account_id_match:
                                    account_id = account_id_match.group(1)
                                    break

                else:
                    user_not_exist = True
                    all_tds = time_soup.find_all('td', class_='ws-nowrap')
                    last_td = all_tds[-2] if all_tds else None

                    if last_td:
                        display_name = last_td.get_text(strip=True)

                date_elements = time_soup.find_all('td', class_='ws-nowrap creation-date')

                if date_elements:
                    creation_date_element = date_elements[-1].find('span', class_='relativetime')
                    creation_date_str = creation_date_element['title'] if creation_date_element else None
                    creation_date_norm = datetime.strptime(creation_date_str, "%Y-%m-%d %H:%M:%SZ")
                    GMT_creation = GMT_zone.localize(creation_date_norm)
                    zone = GMT_creation.astimezone(pytz.UTC)
                    creation_date = int(zone.timestamp())

            body = None
            if withbody:
                body_element = summary.find("div", class_="s-prose")
                if body_element:
                    body = body_element.get_text(strip=True)
                
            owner_data = OrderedDict()

            if user_not_exist:
                owner_data['user_type'] = "does_not_exist"
                owner_data['display_name'] = display_name
            else:

                owner_data = {
                    "account_id": account_id,
                    "reputation": reputation,
                    "user_id": user_id,
                    "user_type": user_type,
                    "profile_image": profile_image,
                    "display_name": display_name,
                    "link": profile_link,
                }

            migration_data = OrderedDict()

            if migrated_id:
                migration_data["question_id"] = migrated_id
            
            if migrated_date:
                migration_data["on_date"] = migrated_date

            if migrated_link:
                migration_data["site_url"] = migrated_link
                

            question_data = OrderedDict()

            question_data["tags"] = tags

            if migrated_link or migrated_id or migrated_date:
                question_data["migrated_from"] = migration_data

            question_data["owner"] = owner_data
            question_data["is_answered"] = is_answered
            question_data["view_count"] = view_count
            question_data["answer_count"] = answer_count
            
            if community_owned_date:
                question_data['community_owned_date'] = community_owned_date


            question_data["score"] = score
            question_data["last_activity_date"] = last_activity_date
            question_data["creation_date"] = creation_date

            if last_edited_date:
                question_data["last_edited_date"] = last_edited_date

            if bounty_amount:
                question_data['bounty_amount'] = bounty_amount

            if bounty_closes_date:
                question_data["bounty_closes_date"] = bounty_closes_date

            if closed_date:
                question_data["closed_date"] = closed_date

            if protected_date:
                question_data["protected_date"] = protected_date

            if locked_date:
                question_data["locked_date"] = locked_date

            question_data["question_id"] = question_id
            question_data["link"] = link

            if closed_reason:
                question_data["closed_reason"] = closed_reason

            question_data["title"] = title

            if withbody and body:
                question_data["body"] = body
          
            items.append(question_data)

    if total:
        return jsonify({"total": len(items)})
    
    sort_key_mapping = {
    'activity': 'last_activity_date',
    'creation': 'creation_date',
    'votes': 'score'
    }

    sort_key = sort_key_mapping.get(sort)
    
    if sort_key:
        if sort in ['votes', 'creation']:
            if min_value is not None:
                items = [item for item in items if item[sort_key] >= min_value]
            if max_value is not None:
                items = [item for item in items if item[sort_key] <= max_value]

        if order == 'asc':
            items.sort(key=lambda x: x.get(sort_key, 0))
        else:
            items.sort(key=lambda x: x.get(sort_key, 0), reverse=True)

    return Response(json.dumps({"items": items}, indent=4), mimetype='application/json')






@app.route('/collectives', methods=['GET'])
def get_all_collectives():
    url = "https://stackoverflow.com/collectives-all"
    response = make_request(url)

    soup = BeautifulSoup(response.content, "html.parser")


    order = request.args.get('order', default='desc')

    descriptions = []
    span_tags = soup.find_all("span", class_="fs-body1 v-truncate2 ow-break-word")
    for span in span_tags:
        descriptions.append(span.text.strip())

    a_tags = soup.find_all("a", class_="js-gps-track", href=True)

    collectives = []
    for a_tag in soup.find_all("a", class_="js-gps-track", href=True):
        href = a_tag.get("href")
        if "/collectives/" in href:
            slug = href.split("/")[-1]
            name = a_tag.text.strip() 
            collectives.append({
                "slug": slug,
                "name": name,
                "link": "https://stackoverflow.com" + href
            })

    collectives = collectives[:9]

    all_collectives = []

    for i, collective in enumerate(collectives):
        link1 = collective["link"]
        slug = collective["slug"]
        name = collective["name"]

        col_text = make_request(link1)
        col_soup = BeautifulSoup(col_text.content, "html.parser")

        # Get the tags
        tags = []
        tag_elements = col_soup.find_all("a", class_="post-tag")
        for tag in tag_elements:
            tags.append(tag.text.strip())


        # Get the external links
        external_links = []
        optgroup = col_soup.find("optgroup", label="External links")
        if optgroup:
            options = optgroup.find_all("option")
            for option in options:
                link2 = option.get("data-url")
                link_type = option.text.strip().lower()
                external_links.append({
                    "type": link_type,
                    "link": link2
                })

        description = descriptions[i] if i < len(descriptions) else ""
    
        collective_info = {
            "tags": tags,
            "external_links": external_links,
            "description": description,
            "link": link1,
            "name": name,
            "slug": slug
        }

        all_collectives.append(collective_info)

        all_collectives.sort(key=lambda x: x['name'], reverse=(order == 'desc'))


    return jsonify(all_collectives)







@app.route('/questions/<question_ids>', methods=['GET'])
def get_questionID(question_ids):

    site = request.args.get('site')
    if not site or site.lower() != 'stackoverflow':
        return jsonify({"error": "The 'site' parameter is required and must be 'stackoverflow'"}), 400

    question_ids_list = question_ids.split(';')
      
    min_value = request.args.get('min', type=int)
    max_value = request.args.get('max', type=int)
    sort = request.args.get('sort', default='activity')
    order = request.args.get('order', default='desc')
    filter = request.args.get('filter', default='default')

    withbody = False
    total = False

    if sort not in ['activity', 'creation', 'votes']:
        return jsonify({"error": "Invalid sort type"}), 400

    if filter not in ['default', 'withbody', 'none', 'total']:
        return jsonify({"error": "Invalid filter type"}), 400
    if filter == "none":
        return jsonify({})
    if filter == "withbody":
        withbody = True
    if filter == "total":
        total = True

    items = []

    for question_id in question_ids_list:
        url = f"https://stackoverflow.com/questions/{question_id}"

        response = make_request(url)

        if response.status_code != 200:
            return jsonify({"error": "Failed to fetch the question from Stack Overflow"}), 500

        soup = BeautifulSoup(response.content, "html.parser")

        aside_element = soup.find('aside', class_='js-bounty-notification')

        bounty_amount = None
        bounty_closes_date = None

        if aside_element:
            date_span = aside_element.find('span', title=True)
            date_bounty_str = date_span['title'] if date_span else None

            date_bounty_norm = datetime.strptime(date_bounty_str, "%Y-%m-%d %H:%M:%SZ")
            GMT_creation = GMT_zone.localize(date_bounty_norm)
            zone = GMT_creation.astimezone(pytz.UTC)
            bounty_closes_date = int(zone.timestamp())

            bounty_span = aside_element.find('span', class_='s-badge__bounty')
            bounty_amount = int(bounty_span.text.strip().replace('+', '')) if bounty_span else None

        summary = soup.find("div", class_="question")

        if summary:

            score = 0

            div_tag = summary.find('div', class_='js-voting-container')
            quest_id = int(div_tag['data-post-id']) if div_tag else None

            div_tag = summary.find('div', class_='js-vote-count')
            score = int(div_tag['data-value']) if div_tag else None

            user_card = summary.find("div", class_="user-details", itemprop="author") 
            profile_link = "aa"
            display_name = ""

            anchor_tag = user_card.find("a") if user_card else None

            if anchor_tag and anchor_tag.get("href"):
                profile_link = "https://stackoverflow.com" + anchor_tag["href"]
                display_name = anchor_tag.text.strip() 
                            

            last_edited_date = None

            edited_element = summary.find('a', title="show all edits to this post")
            if edited_element:
                last_edited_date_str = edited_element.find('span', class_='relativetime')['title']
                last_edited_date_norm = datetime.strptime(last_edited_date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_last_edited = GMT_zone.localize(last_edited_date_norm)
                last_edited_date = int(GMT_last_edited.astimezone(pytz.UTC).timestamp())


            modified_date_element = soup.find('a', href="?lastactivity")
            modified_date = modified_date_element['title'] if modified_date_element else None

            if modified_date:
                modified_date_norm = datetime.strptime(modified_date, "%Y-%m-%d %H:%M:%SZ")
                GMT_last = GMT_zone.localize(modified_date_norm)
                zone = GMT_last.astimezone(pytz.UTC)
                last_activity_date = int(zone.timestamp())

            
            tag_elements = summary.find_all("a", class_="post-tag")
            tags = [tag.text.strip() for tag in tag_elements]

           
            view_tag = soup.find('div', class_='flex--item ws-nowrap mb8')

        
            if view_tag and 'title' in view_tag.attrs:
                title = view_tag['title']
                view_str = title.split()[1].replace(',', '')
                view_count = int(view_str)
            else:
                view_count = None

            answer_count = len(soup.find_all("div", class_="answer"))

            if answer_count > 0:
                is_answered = True
            else:
                is_answered = False

            title = soup.find("a", class_="question-hyperlink").text.strip()

            link = url
            
            time_url = "https://stackoverflow.com/posts/" + str(question_id) + "/timeline"

            response_date = make_request(time_url)
            if response_date.status_code == 200:
                time_soup = BeautifulSoup(response_date.content, 'html.parser')   
                
           
                closed_date = None
                closed_reason = None
                protected_date = None
                locked_date = None
                community_owned_date = None
                unprotected = True

                migrated_date = None
                migrated_id = None
                migrated_link = None
        
                for row in time_soup.find_all('tr'):
                    
                    date_element = row.find('span', class_='relativetime')
                    date_str = date_element['title'] if date_element else None

                    if not date_str:
                        continue


                    date_norm = datetime.strptime(date_str, "%Y-%m-%d %H:%M:%SZ")
                    GMT_creation = GMT_zone.localize(date_norm)
                    zone = GMT_creation.astimezone(pytz.UTC)
                    date = int(zone.timestamp())

                    event_cell = row.find('td', class_='wmn1')
                    if event_cell:
                        event_text = event_cell.text.strip().lower()

                
                        if "migrated" == event_text:
                            migrated_date = date

                            comment_cell = row.find('td', class_='event-comment')
                            if comment_cell:
                                link_tag = comment_cell.find('a')
                                if link_tag and 'href' in link_tag.attrs:
                                    migrated_link = link_tag['href']
                                    match = re.search(r'/questions/(\d+)/', migrated_link)
                                    if match:
                                        migrated_id = int(match.group(1))
                                    else:
                                        migrated_id = None 


                        if 'unprotected' == event_text:
                            unprotected = False

                        if 'closed' == event_text and not closed_date:
                            closed_date = date
                            comment_element = row.find('td', class_='event-comment')
                            closed_reason = comment_element.text.strip() if comment_element else None

                        if unprotected:
                            if 'protected' == event_text and not protected_date:
                                protected_date = date

                        if 'locked' in event_text and not locked_date:
                            locked_date = date

                        if "made wiki" == event_text:
                            community_owned_date = date
    
                user_not_exist = False
                display_name = ""
                profile_elements = time_soup.find_all('a', class_='comment-user owner')

               
                if profile_elements:
                  
                    last_profile_element = profile_elements[-1]
                    display_name = last_profile_element.text.strip()

                    if profile_link == "aa":
                        profile_link = "https://stackoverflow.com" + last_profile_element['href']

            
                    acc = make_request(profile_link)

                    if response.status_code == 200:
                        acc_soup = BeautifulSoup(acc.content, "html.parser")

                        img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                        profile_image = img_tag['src'] if img_tag else None

                        user_id = profile_link.split("/")[-2]
                        user_type = "registered"

                        reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                        if "m" in reputation_text:
                            reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                        elif "k" in reputation_text:
                            reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                        else:
                            reputation = int(reputation_text.replace(',', ''))

                        reputation = int(reputation_text.replace(',', ''))

                        script_tags = acc_soup.find_all("script")
                        account_id = None

                        for script in script_tags:
                            script_text = script.string
                            if script_text and "StackExchange.user.init" in script_text:
                    
                                account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                                if account_id_match:
                                    account_id = account_id_match.group(1)
                                    break

                elif profile_link != "aa":

                    acc = make_request(profile_link)

                    if response.status_code == 200:
                        acc_soup = BeautifulSoup(acc.content, "html.parser")

                        img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                        profile_image = img_tag['src'] if img_tag else None

                        user_id = profile_link.split("/")[-2]
                        user_type = "registered"

                        reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                        if "m" in reputation_text:
                            reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                        elif "k" in reputation_text:
                            reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                        else:
                            reputation = int(reputation_text.replace(',', ''))

                        reputation = int(reputation_text.replace(',', ''))

                        script_tags = acc_soup.find_all("script")
                        account_id = None

                        for script in script_tags:
                            script_text = script.string
                            if script_text and "StackExchange.user.init" in script_text:
                    
                                account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                                if account_id_match:
                                    account_id = account_id_match.group(1)
                                    break

                else:
                    user_not_exist = True
                    all_tds = time_soup.find_all('td', class_='ws-nowrap')
                    last_td = all_tds[-2] if all_tds else None

                    if last_td:
                        display_name = last_td.get_text(strip=True)

                date_elements = time_soup.find_all('td', class_='ws-nowrap creation-date')

                if date_elements:
                    creation_date_element = date_elements[-1].find('span', class_='relativetime')
                    creation_date_str = creation_date_element['title'] if creation_date_element else None
                    creation_date_norm = datetime.strptime(creation_date_str, "%Y-%m-%d %H:%M:%SZ")
                    GMT_creation = GMT_zone.localize(creation_date_norm)
                    zone = GMT_creation.astimezone(pytz.UTC)
                    creation_date = int(zone.timestamp())

            body = None
            if withbody:
                body_element = summary.find("div", class_="s-prose")
                if body_element:
                    body = body_element.get_text(strip=True)
                
            owner_data = OrderedDict()

            if user_not_exist:
                owner_data['user_type'] = "does_not_exist"
                owner_data['display_name'] = display_name
            else:

                owner_data = {
                    "account_id": account_id,
                    "reputation": reputation,
                    "user_id": user_id,
                    "user_type": user_type,
                    "profile_image": profile_image,
                    "display_name": display_name,
                    "link": profile_link,
                }

            migration_data = OrderedDict()

            if migrated_id:
                migration_data["question_id"] = migrated_id
            
            if migrated_date:
                migration_data["on_date"] = migrated_date

            if migrated_link:
                migration_data["site_url"] = migrated_link
                

            question_data = OrderedDict()

            question_data["tags"] = tags

            if migrated_link or migrated_id or migrated_date:
                question_data["migrated_from"] = migration_data

            question_data["owner"] = owner_data
            question_data["is_answered"] = is_answered
            question_data["view_count"] = view_count
            question_data["answer_count"] = answer_count
            
            if community_owned_date:
                question_data['community_owned_date'] = community_owned_date


            question_data["score"] = score
            question_data["last_activity_date"] = last_activity_date
            question_data["creation_date"] = creation_date

            if last_edited_date:
                question_data["last_edited_date"] = last_edited_date

            if bounty_amount:
                question_data['bounty_amount'] = bounty_amount

            if bounty_closes_date:
                question_data["bounty_closes_date"] = bounty_closes_date

            if closed_date:
                question_data["closed_date"] = closed_date

            if protected_date:
                question_data["protected_date"] = protected_date

            if locked_date:
                question_data["locked_date"] = locked_date

            question_data["question_id"] = question_id
            question_data["link"] = link

            if closed_reason:
                question_data["closed_reason"] = closed_reason

            question_data["title"] = title

            if withbody and body:
                question_data["body"] = body
          
            items.append(question_data)

    if total:
        return jsonify({"total": len(items)})
    
    sort_key_mapping = {
    'activity': 'last_activity_date',
    'creation': 'creation_date',
    'votes': 'score'
    }

    sort_key = sort_key_mapping.get(sort)
    
    if sort_key:
        if sort in ['votes', 'creation']:
            if min_value is not None:
                items = [item for item in items if item[sort_key] >= min_value]
            if max_value is not None:
                items = [item for item in items if item[sort_key] <= max_value]

        if order == 'asc':
            items.sort(key=lambda x: x.get(sort_key, 0))
        else:
            items.sort(key=lambda x: x.get(sort_key, 0), reverse=True)

    return Response(json.dumps({"items": items}, indent=4), mimetype='application/json')

    







@app.route('/answers/<answerIDs>', methods=['GET'])
def get_answerID(answerIDs):

    site = request.args.get('site')
    if not site or site.lower() != 'stackoverflow':
        return jsonify({"error": "The 'site' parameter is required and must be 'stackoverflow'"}), 400



    answer_ids_list = answerIDs.split(';')

    min_value = request.args.get('min', type=int)
    max_value = request.args.get('max', type=int)
    sort = request.args.get('sort', default='activity')
    order = request.args.get('order', default='desc')
    filter = request.args.get('filter', default='default')

    withbody = False
    total = False

    if sort not in ['activity', 'creation', 'votes']:
        return jsonify({"error": "Invalid sort type"}), 400

    if filter not in ['default', 'withbody', 'none', 'total']:
        return jsonify({"error": "Invalid filter type"}), 400
    if filter == "none":
        return jsonify({})
    if filter == "withbody":
        withbody = True
    if filter == "total":
        total = True




    items = []

    for answerID in answer_ids_list:

        url = f"https://stackoverflow.com/a/{answerID}"
        response = make_request(url)

        if response.status_code != 200:
            return jsonify({"error": "Failed to retrieve the answer"}), 404
        

        soup = BeautifulSoup(response.content, "html.parser")
        answer_summary = soup.find("div", {"id": f"answer-{answerID}"})

        if answer_summary:

            collective_div = soup.find('div', class_='themed-tc')
            collective_check = False

            if collective_div:
                collective_link_tag = collective_div.find('a')
                if collective_link_tag and 'href' in collective_link_tag.attrs:
                    collective_check = True
                    collective_link = "https://stackoverflow.com" + collective_link_tag['href']
                    collective_name = collective_link_tag.text.strip()
                    collective_slug = collective_link_tag['href'].split('/')[-1]

                    col_text = make_request(collective_link)
                    col_soup = BeautifulSoup(col_text.content, "html.parser")

                    tags = []
                    tag_elements = col_soup.find_all("a", class_="post-tag")
                    for tag in tag_elements:
                        tags.append(tag.text.strip())

                    description_div = col_soup.find('div', class_='fs-body1 fc-black-500 d:fc-black-600 mb6 wmx7')

                    collective_description = ""
                    if description_div:
                        collective_description = description_div.text.strip()

                    external_links = []
                    optgroup = col_soup.find("optgroup", label="External links")
                    if optgroup:
                        options = optgroup.find_all("option")
                        for option in options:
                            link2 = option.get("data-url")
                            link_type = option.text.strip().lower()
                            external_links.append({
                                "type": link_type,
                                "link": link2
                            })

                    collective_info = {
                        "tags": tags,
                        "external_links": external_links,
                        "description": collective_description,
                        "link": collective_link,
                        "name": collective_name,
                        "slug": collective_slug
                    }


            user_card = answer_summary.find("div", class_="user-details", itemprop="author") 

            profile_link = "aa"
            display_name = ""

            anchor_tag = user_card.find("a") if user_card else None

            if anchor_tag and anchor_tag.get("href"):
                profile_link = "https://stackoverflow.com" + anchor_tag["href"]
                display_name = anchor_tag.text.strip() 


            score = 0

            div_tag = soup.find('div', class_='js-voting-container')
            quest_id = int(div_tag['data-post-id']) if div_tag else None

            div_tag = answer_summary.find('div', class_='js-vote-count')
            score = int(div_tag['data-value']) if div_tag else None


            is_accepted = False

            accepted_indicator = answer_summary.find('div', class_='js-accepted-answer-indicator')

            if accepted_indicator and 'd-none' not in accepted_indicator.get('class', []):
                is_accepted = True

            if score > 0:
                is_accepted = True



        edited_element = answer_summary.find('a', title="show all edits to this post")
        if edited_element:
            last_edited_date_str = edited_element.find('span', class_='relativetime')['title']
            last_edited_date_norm = datetime.strptime(last_edited_date_str, "%Y-%m-%d %H:%M:%SZ")
            GMT_last_edited = GMT_zone.localize(last_edited_date_norm)
            last_edited_date = int(GMT_last_edited.astimezone(pytz.UTC).timestamp())


        time_url = "https://stackoverflow.com/posts/" + str(answerID) + "/timeline"

        response_date = make_request(time_url)
        if response_date.status_code == 200:
            time_soup = BeautifulSoup(response_date.content, 'html.parser')    


 
            locked_date = None
            community_owned_date = None
        
    
            for row in time_soup.find_all('tr'):
                
                date_element = row.find('span', class_='relativetime')
                date_str = date_element['title'] if date_element else None

                if not date_str:
                    continue


                date_norm = datetime.strptime(date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_creation = GMT_zone.localize(date_norm)
                zone = GMT_creation.astimezone(pytz.UTC)
                date = int(zone.timestamp())

                event_cell = row.find('td', class_='wmn1')
                if event_cell:
                    event_text = event_cell.text.strip().lower()

                    if 'locked' in event_text and not locked_date:
                        locked_date = date

                    if "made wiki" == event_text:
                        community_owned_date = date


            user_not_exist = False
            display_name = ""
            profile_elements = time_soup.find_all('a', class_='comment-user owner')

            
            if profile_elements:
                
                last_profile_element = profile_elements[-1]
                display_name = last_profile_element.text.strip()

                if profile_link == "aa":
                    profile_link = "https://stackoverflow.com" + last_profile_element['href']

        
                acc = make_request(profile_link)

                if response.status_code == 200:
                    acc_soup = BeautifulSoup(acc.content, "html.parser")

                    img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                    profile_image = img_tag['src'] if img_tag else None

                    user_id = profile_link.split("/")[-2]
                    user_type = "registered"

                    reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                    if "m" in reputation_text:
                        reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                    elif "k" in reputation_text:
                        reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                    else:
                        reputation = int(reputation_text.replace(',', ''))

                    reputation = int(reputation_text.replace(',', ''))

                    script_tags = acc_soup.find_all("script")
                    account_id = None

                    for script in script_tags:
                        script_text = script.string
                        if script_text and "StackExchange.user.init" in script_text:
                
                            account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                            if account_id_match:
                                account_id = account_id_match.group(1)
                                break

            elif profile_link != "aa":

                acc = make_request(profile_link)

                if response.status_code == 200:
                    acc_soup = BeautifulSoup(acc.content, "html.parser")

                    img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                    profile_image = img_tag['src'] if img_tag else None

                    user_id = profile_link.split("/")[-2]
                    user_type = "registered"

                    reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                    if "m" in reputation_text:
                        reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                    elif "k" in reputation_text:
                        reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                    else:
                        reputation = int(reputation_text.replace(',', ''))

                    reputation = int(reputation_text.replace(',', ''))

                    script_tags = acc_soup.find_all("script")
                    account_id = None

                    for script in script_tags:
                        script_text = script.string
                        if script_text and "StackExchange.user.init" in script_text:
                
                            account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                            if account_id_match:
                                account_id = account_id_match.group(1)
                                break

            else:
                user_not_exist = True
                all_tds = time_soup.find_all('td', class_='ws-nowrap')
                last_td = all_tds[-2] if all_tds else None

                if last_td:
                    display_name = last_td.get_text(strip=True)




            last_history_date = None

            for row in time_soup.find_all('tr', {'data-eventtype': 'history'}):
                user_cell = row.find('a', class_='comment-user')
                date = row.find('span', class_='relativetime')['title']

                if user_cell and user_cell.get('href') == '/users/-1/community':
                    continue

                if user_cell and 'comment-user' in user_cell.get('class', []):
                    if not last_history_date or date > last_history_date:
                        last_history_date = date
                          

            if last_history_date:
                last_history_date_norm = datetime.strptime(last_history_date, "%Y-%m-%d %H:%M:%SZ")
                GMT_last = GMT_zone.localize(last_history_date_norm)
                zone = GMT_last.astimezone(pytz.UTC)
                last_activity_date = int(zone.timestamp())
            else:
                last_activity_date = ""

            date_elements = time_soup.find_all('td', class_='ws-nowrap creation-date')

            if date_elements:
                creation_date_element = date_elements[-1].find('span', class_='relativetime')
                creation_date_str = creation_date_element['title'] if creation_date_element else None
                creation_date_norm = datetime.strptime(creation_date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_creation = GMT_zone.localize(creation_date_norm)
                zone = GMT_creation.astimezone(pytz.UTC)
                creation_date = int(zone.timestamp())

            body = None
            if withbody:
                body_element = answer_summary.find("div", class_="s-prose")
                if body_element:
                    body = body_element.get_text(strip=True)


            owner_data = OrderedDict()

            if user_not_exist:
                owner_data['user_type'] = "does_not_exist"
                owner_data['display_name'] = display_name
            else:

                owner_data = {
                    "account_id": int(account_id),
                    "reputation": reputation,
                    "user_id": int(user_id),
                    "user_type": user_type,
                    "profile_image": profile_image,
                    "display_name": display_name,
                    "link": profile_link,
                }

            answer_data = OrderedDict()

            if collective_check:
                answer_data["recommendations"] = [{"collective": collective_info}]

            answer_data["owner"] = owner_data
            answer_data["is_accepted"] = is_accepted

            if community_owned_date:
                answer_data["community_owned_date"] = community_owned_date

            if locked_date:
                answer_data["locked_date"] = locked_date

            answer_data["score"] = score
            answer_data["last_activity_date"] = last_activity_date

            if edited_element:
                answer_data["last_edited_date"] = last_edited_date

            answer_data["creation_date"] = creation_date  
            answer_data[ "answer_id"] = int(answerID)
            answer_data["question_id"] = quest_id

            if body:
                answer_data["body"] = body

            items.append(answer_data)

    if total:
        return jsonify({"total": len(items)})

    sort_key_mapping = {
    'activity': 'last_activity_date',
    'creation': 'creation_date',
    'votes': 'score'
    }

    sort_key = sort_key_mapping.get(sort)
    

    if sort_key:
        if sort in ['votes', 'creation']:
            if min_value is not None:
                items = [item for item in items if item[sort_key] >= min_value]
            if max_value is not None:
                items = [item for item in items if item[sort_key] <= max_value]

        if order == 'asc':
            items.sort(key=lambda x: x.get(sort_key, 0))
        else:
            items.sort(key=lambda x: x.get(sort_key, 0), reverse=True)

    return Response(json.dumps({"items": items}, indent=4), mimetype='application/json')







@app.route('/questions/<question_ids>/answers', methods=['GET'])
def get_question_answers(question_ids):


    site = request.args.get('site')
    if not site or site.lower() != 'stackoverflow':
        return jsonify({"error": "The 'site' parameter is required and must be 'stackoverflow'"}), 400


    question_ids_list = question_ids.split(';')

    min_value = request.args.get('min', type=int)
    max_value = request.args.get('max', type=int)
    sort = request.args.get('sort', default='activity')
    order = request.args.get('order', default='desc')
    filter = request.args.get('filter', default='default')

    withbody = False
    total = False

    if sort not in ['activity', 'creation', 'votes']:
        return jsonify({"error": "Invalid sort type"}), 400

    if filter not in ['default', 'withbody', 'none', 'total']:
        return jsonify({"error": "Invalid filter type"}), 400
    if filter == "none":
        return jsonify({})
    if filter == "withbody":
        withbody = True
    if filter == "total":
        total = True

    
    items = []


    for questionID in question_ids_list:
        url = f"https://stackoverflow.com/questions/{questionID}"

    
        response = make_request(url)

        if response.status_code != 200:
            return jsonify({"error": "Failed to retrieve the data"}), 404

        soup = BeautifulSoup(response.content, "html.parser")
        answers_summary = soup.find_all("div", class_="answer")

        for answer_summary in answers_summary:


            answerID = answer_summary.get("data-answerid")

            collective_div = soup.find('div', class_='themed-tc')
            collective_check = False

            if collective_div:
                collective_link_tag = collective_div.find('a')
                if collective_link_tag and 'href' in collective_link_tag.attrs:
                    collective_check = True
                    collective_link = "https://stackoverflow.com" + collective_link_tag['href']
                    collective_name = collective_link_tag.text.strip()
                    collective_slug = collective_link_tag['href'].split('/')[-1]

                    col_text = make_request(collective_link)
                    col_soup = BeautifulSoup(col_text.content, "html.parser")

                    tags = []
                    tag_elements = col_soup.find_all("a", class_="post-tag")
                    for tag in tag_elements:
                        tags.append(tag.text.strip())

                    description_div = col_soup.find('div', class_='fs-body1 fc-black-500 d:fc-black-600 mb6 wmx7')

                    collective_description = ""
                    if description_div:
                        collective_description = description_div.text.strip()

                    external_links = []
                    optgroup = col_soup.find("optgroup", label="External links")
                    if optgroup:
                        options = optgroup.find_all("option")
                        for option in options:
                            link2 = option.get("data-url")
                            link_type = option.text.strip().lower()
                            external_links.append({
                                "type": link_type,
                                "link": link2
                            })

                    collective_info = {
                        "tags": tags,
                        "external_links": external_links,
                        "description": collective_description,
                        "link": collective_link,
                        "name": collective_name,
                        "slug": collective_slug
                    }


            user_card = answer_summary.find("div", class_="user-details", itemprop="author") 

            profile_link = "aa"
            display_name = ""

            anchor_tag = user_card.find("a") if user_card else None

            if anchor_tag and anchor_tag.get("href"):
                profile_link = "https://stackoverflow.com" + anchor_tag["href"]
                display_name = anchor_tag.text.strip() 


            score = 0

            div_tag = soup.find('div', class_='js-voting-container')
            quest_id = int(div_tag['data-post-id']) if div_tag else None

            div_tag = answer_summary.find('div', class_='js-vote-count')
            score = int(div_tag['data-value']) if div_tag else None


            is_accepted = False

            accepted_indicator = answer_summary.find('div', class_='js-accepted-answer-indicator')

            if accepted_indicator and 'd-none' not in accepted_indicator.get('class', []):
                is_accepted = True

            if score > 0:
                is_accepted = True



        edited_element = answer_summary.find('a', title="show all edits to this post")
        if edited_element:
            last_edited_date_str = edited_element.find('span', class_='relativetime')['title']
            last_edited_date_norm = datetime.strptime(last_edited_date_str, "%Y-%m-%d %H:%M:%SZ")
            GMT_last_edited = GMT_zone.localize(last_edited_date_norm)
            last_edited_date = int(GMT_last_edited.astimezone(pytz.UTC).timestamp())


        time_url = "https://stackoverflow.com/posts/" + answerID + "/timeline"

        response_date = make_request(time_url)
        if response_date.status_code == 200:
            time_soup = BeautifulSoup(response_date.content, 'html.parser')    


 
            locked_date = None
            community_owned_date = None
        
    
            for row in time_soup.find_all('tr'):
                
                date_element = row.find('span', class_='relativetime')
                date_str = date_element['title'] if date_element else None

                if not date_str:
                    continue


                date_norm = datetime.strptime(date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_creation = GMT_zone.localize(date_norm)
                zone = GMT_creation.astimezone(pytz.UTC)
                date = int(zone.timestamp())

                event_cell = row.find('td', class_='wmn1')
                if event_cell:
                    event_text = event_cell.text.strip().lower()

                    if 'locked' in event_text and not locked_date:
                        locked_date = date

                    if "made wiki" == event_text:
                        community_owned_date = date


            user_not_exist = False
            display_name = ""
            profile_elements = time_soup.find_all('a', class_='comment-user owner')

            
            if profile_elements:
                
                last_profile_element = profile_elements[-1]
                display_name = last_profile_element.text.strip()

                if profile_link == "aa":
                    profile_link = "https://stackoverflow.com" + last_profile_element['href']

        
                acc = make_request(profile_link)

                if response.status_code == 200:
                    acc_soup = BeautifulSoup(acc.content, "html.parser")

                    img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                    profile_image = img_tag['src'] if img_tag else None

                    user_id = profile_link.split("/")[-2]
                    user_type = "registered"

                    reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                    if "m" in reputation_text:
                        reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                    elif "k" in reputation_text:
                        reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                    else:
                        reputation = int(reputation_text.replace(',', ''))

                    reputation = int(reputation_text.replace(',', ''))

                    script_tags = acc_soup.find_all("script")
                    account_id = None

                    for script in script_tags:
                        script_text = script.string
                        if script_text and "StackExchange.user.init" in script_text:
                
                            account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                            if account_id_match:
                                account_id = account_id_match.group(1)
                                break

            elif profile_link != "aa":

                acc = make_request(profile_link)

                if response.status_code == 200:
                    acc_soup = BeautifulSoup(acc.content, "html.parser")

                    img_tag = acc_soup.find('div', class_='bar-md bs-sm').find('img')
                    profile_image = img_tag['src'] if img_tag else None

                    user_id = profile_link.split("/")[-2]
                    user_type = "registered"

                    reputation_text = acc_soup.find('div', class_='fs-body3 fc-black-600').text
                    if "m" in reputation_text:
                        reputation = int(float(reputation_text.replace('m', '').replace(',', '')) * 1_000_000)
                    elif "k" in reputation_text:
                        reputation = int(float(reputation_text.replace('k', '').replace(',', '')) * 1000)
                    else:
                        reputation = int(reputation_text.replace(',', ''))

                    reputation = int(reputation_text.replace(',', ''))

                    script_tags = acc_soup.find_all("script")
                    account_id = None

                    for script in script_tags:
                        script_text = script.string
                        if script_text and "StackExchange.user.init" in script_text:
                
                            account_id_match = re.search(r'accountId:\s*(\d+)', script_text)
                            if account_id_match:
                                account_id = account_id_match.group(1)
                                break

            else:
                user_not_exist = True
                all_tds = time_soup.find_all('td', class_='ws-nowrap')
                last_td = all_tds[-2] if all_tds else None

                if last_td:
                    display_name = last_td.get_text(strip=True)




            last_history_date = None

            for row in time_soup.find_all('tr', {'data-eventtype': 'history'}):
                user_cell = row.find('a', class_='comment-user')
                date = row.find('span', class_='relativetime')['title']

                if user_cell and user_cell.get('href') == '/users/-1/community':
                    continue

                if user_cell and 'comment-user' in user_cell.get('class', []):
                    if not last_history_date or date > last_history_date:
                        last_history_date = date
                          

            if last_history_date:
                last_history_date_norm = datetime.strptime(last_history_date, "%Y-%m-%d %H:%M:%SZ")
                GMT_last = GMT_zone.localize(last_history_date_norm)
                zone = GMT_last.astimezone(pytz.UTC)
                last_activity_date = int(zone.timestamp())
            else:
                last_activity_date = ""

            date_elements = time_soup.find_all('td', class_='ws-nowrap creation-date')

            if date_elements:
                creation_date_element = date_elements[-1].find('span', class_='relativetime')
                creation_date_str = creation_date_element['title'] if creation_date_element else None
                creation_date_norm = datetime.strptime(creation_date_str, "%Y-%m-%d %H:%M:%SZ")
                GMT_creation = GMT_zone.localize(creation_date_norm)
                zone = GMT_creation.astimezone(pytz.UTC)
                creation_date = int(zone.timestamp())

            body = None
            if withbody:
                body_element = answer_summary.find("div", class_="s-prose")
                if body_element:
                    body = body_element.get_text(strip=True)


            owner_data = OrderedDict()

            if user_not_exist:
                owner_data['user_type'] = "does_not_exist"
                owner_data['display_name'] = display_name
            else:

                owner_data = {
                    "account_id": int(account_id),
                    "reputation": reputation,
                    "user_id": int(user_id),
                    "user_type": user_type,
                    "profile_image": profile_image,
                    "display_name": display_name,
                    "link": profile_link,
                }

            answer_data = OrderedDict()

            if collective_check:
                answer_data["recommendations"] = [{"collective": collective_info}]

            answer_data["owner"] = owner_data
            answer_data["is_accepted"] = is_accepted

            if community_owned_date:
                answer_data["community_owned_date"] = community_owned_date

            if locked_date:
                answer_data["locked_date"] = locked_date

            answer_data["score"] = score
            answer_data["last_activity_date"] = last_activity_date

            if edited_element:
                answer_data["last_edited_date"] = last_edited_date

            answer_data["creation_date"] = creation_date  
            answer_data[ "answer_id"] = int(answerID)
            answer_data["question_id"] = quest_id

            if body:
                answer_data["body"] = body

            items.append(answer_data)

    if total:
        return jsonify({"total": len(items)})

    sort_key_mapping = {
    'activity': 'last_activity_date',
    'creation': 'creation_date',
    'votes': 'score'
    }

    sort_key = sort_key_mapping.get(sort)
    

    if sort_key:
        if sort in ['votes', 'creation']:
            if min_value is not None:
                items = [item for item in items if item[sort_key] >= min_value]
            if max_value is not None:
                items = [item for item in items if item[sort_key] <= max_value]

        if order == 'asc':
            items.sort(key=lambda x: x.get(sort_key, 0))
        else:
            items.sort(key=lambda x: x.get(sort_key, 0), reverse=True)

    return Response(json.dumps({"items": items}, indent=4), mimetype='application/json')






if __name__ == '__main__':
    port = int(os.environ.get('STACKOVERFLOW_API_PORT', 5000))
    app.run(port=port)
    