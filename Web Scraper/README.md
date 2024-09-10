# README

## **Overview**
This is a Python web application built using the **Flask** framework. The app scrapes data from **Stack Overflow** to recreate several API endpoints that return information about questions, answers, and collectives. It uses **BeautifulSoup** for HTML parsing and allows filtering and sorting through query parameters. Each endpoint supports the four built-in filters available in the **Stack Exchange API**.

## **Key Endpoints**

- **GET `/questions`**:  
  Retrieves a list of questions from Stack Overflow, filtered by tags, sorting options, and other parameters.

- **GET `/questions/<question_ids>`**:  
  Retrieves detailed information about specific questions.

- **GET `/questions/<question_ids>/answers`**:  
  Fetches answers for a specific question.

- **GET `/answers/<answerIDs>`**:  
  Retrieves detailed information about specific answers.

- **GET `/collectives`**:  
  Retrieves information about Stack Overflow collectives.

## **Installation**

1. Clone the repository.
2. Install dependencies:
   ```bash
   pip install Flask requests beautifulsoup4 pytz



## **Usage**
To fetch a list of questions:
GET /questions?site=stackoverflow&tagged=python&sort=activity
