This is a Python web application built using the Flask framework. The app scrapes data from Stack Overflow to recreate several API endpoints that return information about questions, answers, and collectives. It uses BeautifulSoup for HTML parsing and allows filtering and sorting through query parameters. Each endpoint has the 4 built in filters that are available in the Stack Exchange API.


Key Endpoints:
GET /questions: Retrieves a list of questions from Stack Overflow, filtered by tags, sorting options, and other parameters.

GET /questions/<question_ids>: Retrieves detailed information about specific questions.

GET /questions/<question_ids>/answers: Fetches answers for a specific question.

GET /answers/<answerIDs>: Retrieves detailed information about specific answers.

GET /collectives: Retrieves information about Stack Overflow collectives.



Installation:
Clone the repository.
Install dependencies:
pip install Flask requests beautifulsoup4 pytz

Run the Flask app:
python stackoverflow_scraper.py


Usage
To fetch a list of questions:
GET /questions?site=stackoverflow&tagged=python&sort=activity