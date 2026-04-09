# 🏥 Online Doctor Appointment Booking System

A secure, high-performance web-based medical scheduling platform built
in **C++ using the Crow framework**. The system allows patients to
browse doctors by specialty, check real-time availability, and book or
cancel appointments efficiently while ensuring strong security and
database integrity.

**Author:** Ahtesham Latif\
GitHub: https://github.com/Ahtesham-Latif

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Framework](https://img.shields.io/badge/Framework-Crow-green.svg)
![Database](https://img.shields.io/badge/Database-SQLite3-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

------------------------------------------------------------------------

# ✨ Key Features

## 👨‍⚕️ Doctor Directory

Browse doctors categorized by medical specialties such as: -
Cardiologist - Dentist - Dermatologist - General Physician

Each doctor profile displays scheduling information and available
appointment slots.

------------------------------------------------------------------------

## 📅 Real-Time Appointment Scheduling

Patients can: - View available appointment slots - Book appointments
instantly - Cancel existing bookings

The system includes **automatic double-booking protection**.

------------------------------------------------------------------------

## 🔐 Secure Session-Based Navigation

Database IDs are **never exposed in URLs**.

Instead the system uses: - Session storage - Server-side context
validation

This prevents **IDOR (Insecure Direct Object Reference) attacks** where
users manipulate IDs in URLs.

------------------------------------------------------------------------

## ⚡ Asynchronous Email Notifications

The system integrates with **n8n automation workflows**.

After a successful booking or cancellation: 1. The backend triggers a
webhook 2. n8n sends the confirmation email 3. The request runs in a
background thread

This ensures the user sees the confirmation page immediately.

------------------------------------------------------------------------

## 🧠 Smart Database Constraints

The database schema uses a **composite unique constraint**:

(doctor_id, slot_id, appointment_date)

This ensures: - No double booking - Same slot can exist on different
days - Database-level scheduling validation

------------------------------------------------------------------------

## 🎨 Improved User Experience

Frontend improvements include: - Skeleton loaders while data loads -
Elimination of empty white screens - Improved perceived performance

------------------------------------------------------------------------

# 🛠️ Technical Stack

- **Backend Framework:** Crow (C++17)  
- **Database:** SQLite3  
- **Security:** OpenSSL (TLS/SSL)  
- **Templates:** Mustache HTML  
- **Automation:** n8n Webhooks  
- **Build System:** CMake  
- **Platform:** Cross-platform (Windows, Linux, macOS)

# 🧱 System Architecture

The system follows an **MVC (Model-View-Controller)** architecture.

### Models

Represent core entities: - Doctor - Patient - Appointment - Schedule -
Category

### Controllers

Handle business logic such as: - Doctor lookup - Slot availability -
Appointment booking - Appointment cancellation

### Views

Frontend HTML pages served by the Crow server.

Examples: - category.html - doctor.html - appointment.html -
confirmation.html - cancellation.html

### Services

Utility modules for: - Helper functions - HTTP requests - Notification
triggers

------------------------------------------------------------------------

# 📂 Project Structure

crow_backend/ ├── config/ \# Configuration files ├── controllers/ \# MVC
controllers ├── models/ \# Data structures ├── services/ \# Utility
services ├── routes/ \# API route definitions ├── public/ \# Frontend
HTML/CSS ├── db/ \# SQLite database ├── Crow/ \# Crow framework source
├── httplib/ \# HTTP client library ├── CMakeLists.txt \# Build
configuration └── main.cpp \# Application entry point

------------------------------------------------------------------------

# 🔒 Security Features

### HTTPS / TLS Encryption

All traffic is encrypted using **OpenSSL**.

### URL Masking

Sensitive database IDs are **never exposed in URLs**.

### Database Integrity

Composite unique constraint prevents duplicate bookings.

### Input Validation

All endpoints validate user input to prevent invalid requests.

------------------------------------------------------------------------

# 🚀 Installation & Setup

## Prerequisites

-   C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
-   CMake 3.15+
-   SQLite3
-   OpenSSL

Windows additional libraries: - ws2_32 - mswsock

------------------------------------------------------------------------

# ⚙️ Build Instructions

### Clone the Repository

git clone https://github.com/Ahtesham-Latif/crow_server.git cd
crow_server

### Configure SSL Certificates

Place the following files in the project directory:

cert.pem key.pem

### Build the Project

mkdir build cd build cmake .. make

### Run the Server

./server

Server runs at:

https://localhost:8443

------------------------------------------------------------------------

# 🔗 API Endpoints

  Endpoint        Description
  --------------- --------------------------
  /categories     List medical specialties
  /doctors        View doctors by category
  /schedule       View doctor availability
  /appointments   Book appointment
  /cancel         Cancel appointment
  /patients       Manage patient records

------------------------------------------------------------------------

# 📧 Email Automation (n8n)

The backend triggers **webhooks to n8n** for: - Appointment
confirmations - Cancellation notifications - Reminder emails

Webhook URLs are stored as **environment variables**.

------------------------------------------------------------------------

# 📝 License

MIT License

------------------------------------------------------------------------

# 👨‍💻 Author

**Ahtesham Latif**

GitHub: https://github.com/Ahtesham-Latif

------------------------------------------------------------------------

Last Updated: March 2026
