const state = {
  categories: [],
  doctors: [],
  selectedCategoryId: null
};

const elements = {
  categoryReadList: document.getElementById("categoryReadList"),
  categoryDeleteList: document.getElementById("categoryDeleteList"),
  categoryBox: document.getElementById("categoryBox"),
  addCategoryForm: document.getElementById("addCategoryForm"),
  addDoctorForm: document.getElementById("addDoctorForm"),
  doctorReadList: document.getElementById("doctorReadList"),
  doctorDeleteList: document.getElementById("doctorDeleteList"),
  doctorCategorySelect: document.getElementById("doctor_category_id"),
  doctorFilterRead: document.getElementById("doctorFilterRead"),
  doctorFilterDelete: document.getElementById("doctorFilterDelete"),
  editCategoryForm: document.getElementById("editCategoryForm"),
  editDoctorForm: document.getElementById("editDoctorForm"),
  editCategorySelect: document.getElementById("edit_category_id"),
  editDoctorSelect: document.getElementById("edit_doctor_id"),
  editDoctorCategorySelect: document.getElementById("edit_doctor_category_id"),
  editCategoryName: document.getElementById("edit_category_name"),
  editCategoryDescription: document.getElementById("edit_category_description"),
  editDoctorName: document.getElementById("edit_doctor_name"),
  editDoctorPhone: document.getElementById("edit_doctor_phone"),
  editDoctorExperience: document.getElementById("edit_experience_years"),
  editDoctorQualifications: document.getElementById("edit_qualifications"),
  editDoctorRatings: document.getElementById("edit_ratings"),
  selectedCategoryChip: document.getElementById("selectedCategoryChip"),
  toast: document.getElementById("toast")
};

const safePattern = /^[a-zA-Z0-9\s]+$/;
const safeDoctorPattern = /^[a-zA-Z0-9\s\.,-]+$/;

let publicSessionReady = false;
let publicSessionPromise = null;

async function ensurePublicSession() {
  if (publicSessionReady) return;
  if (publicSessionPromise) return publicSessionPromise;

  publicSessionPromise = fetch("/public_session", { credentials: "same-origin" })
    .then(() => {
      publicSessionReady = true;
    })
    .catch((err) => {
      console.error(err);
      publicSessionReady = false;
    })
    .finally(() => {
      publicSessionPromise = null;
    });

  return publicSessionPromise;
}

function showToast(message, type = "success") {
  if (!elements.toast) return;
  elements.toast.textContent = message;
  elements.toast.className = `toast show ${type}`;
  setTimeout(() => {
    elements.toast.className = "toast";
  }, 3000);
}

function getCategoryById(id) {
  return state.categories.find(cat => cat.category_id === id) || null;
}

function getDoctorById(id) {
  return state.doctors.find(doc => doc.doctor_id === id) || null;
}

function fillSelect(selectEl, options) {
  if (!selectEl) return;
  selectEl.innerHTML = "";
  options.forEach(option => {
    const el = document.createElement("option");
    el.value = option.value;
    el.textContent = option.label;
    selectEl.appendChild(el);
  });
}

function renderCategoryBox() {
  if (!elements.categoryBox || !elements.selectedCategoryChip) return;

  if (!state.selectedCategoryId) {
    elements.categoryBox.innerHTML = `
      <h4>Category Box</h4>
      <p>Select a category to see details and add doctors in that category.</p>
    `;
    elements.selectedCategoryChip.textContent = "All categories";
    return;
  }

  const category = getCategoryById(state.selectedCategoryId);
  const count = state.doctors.filter(doc => doc.category_id === state.selectedCategoryId).length;

  if (!category) {
    elements.categoryBox.innerHTML = `
      <h4>Category Box</h4>
      <p>The selected category could not be found. Please refresh the list.</p>
    `;
    return;
  }

  elements.categoryBox.innerHTML = `
    <h4>${category.category_name}</h4>
    <p>${category.description || "No description provided."}</p>
    <p><strong>${count}</strong> doctor${count === 1 ? "" : "s"} in this category.</p>
  `;

  elements.selectedCategoryChip.textContent = category.category_name;
}

function createCategoryItem(category, { showSelect, showDelete }) {
  const item = document.createElement("div");
  item.className = "list-item";

  const count = state.doctors.filter(doc => doc.category_id === category.category_id).length;

  item.innerHTML = `
    <div class="meta">
      <strong>${category.category_name}</strong>
      <span>${category.description || "No description"}</span>
    </div>
    <span class="badge">${count} doctor${count === 1 ? "" : "s"}</span>
  `;

  const actionRow = document.createElement("div");
  actionRow.className = "button-row";

  if (showSelect) {
    const selectBtn = document.createElement("button");
    selectBtn.type = "button";
    selectBtn.className = "btn btn-outline";
    selectBtn.textContent = "Select";
    selectBtn.addEventListener("click", () => {
      state.selectedCategoryId = category.category_id;
      if (elements.doctorCategorySelect) {
        elements.doctorCategorySelect.value = String(category.category_id);
      }
      if (elements.editDoctorCategorySelect) {
        elements.editDoctorCategorySelect.value = String(category.category_id);
      }
      renderCategoryBox();
    });
    actionRow.appendChild(selectBtn);
  }

  if (showDelete) {
    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "btn btn-danger";
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", async () => {
      const ok = confirm(`Please confirm you want to delete "${category.category_name}".`);
      if (!ok) return;

      const res = await fetch(`/delete_category/${category.category_id}`, { method: "DELETE" });
      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Category deleted successfully.");
        if (state.selectedCategoryId === category.category_id) {
          state.selectedCategoryId = null;
        }
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not delete that category. Please try again.", "error");
      }
    });
    actionRow.appendChild(deleteBtn);
  }

  if (actionRow.children.length > 0) {
    item.appendChild(actionRow);
  }

  return item;
}

function renderCategoryReadList() {
  if (!elements.categoryReadList) return;
  elements.categoryReadList.innerHTML = "";

  if (state.categories.length === 0) {
    elements.categoryReadList.innerHTML = `<div class="empty-state">No categories yet. Please add one.</div>`;
    return;
  }

  state.categories.forEach(category => {
    elements.categoryReadList.appendChild(createCategoryItem(category, { showSelect: true, showDelete: false }));
  });
}

function renderCategoryDeleteList() {
  if (!elements.categoryDeleteList) return;
  elements.categoryDeleteList.innerHTML = "";

  if (state.categories.length === 0) {
    elements.categoryDeleteList.innerHTML = `<div class="empty-state">No categories yet. Please add one.</div>`;
    return;
  }

  state.categories.forEach(category => {
    elements.categoryDeleteList.appendChild(createCategoryItem(category, { showSelect: false, showDelete: true }));
  });
}

function createDoctorItem(doc, { showDelete }) {
  const item = document.createElement("div");
  item.className = "list-item";

  const category = getCategoryById(doc.category_id);
  const categoryName = category ? category.category_name : `Category #${doc.category_id}`;
  const experience = doc.experience_years ?? "0";
  const qualification = doc.qualifications || "No qualification";
  const rating = doc.ratings ?? "N/A";

  item.innerHTML = `
    <div class="meta">
      <strong>${doc.doctor_name}</strong>
      <span>${categoryName} · ${experience} yrs ⏳ · ${qualification}</span>
      <span>Rating: ⭐ ${rating}</span>
    </div>
  `;

  if (showDelete) {
    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "btn btn-danger";
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", async () => {
      const ok = confirm(`Please confirm you want to delete "${doc.doctor_name}".`);
      if (!ok) return;

      const res = await fetch(`/delete_doctor/${doc.doctor_id}`, { method: "DELETE" });
      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Doctor deleted successfully.");
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not delete that doctor. Please try again.", "error");
      }
    });

    item.appendChild(deleteBtn);
  }

  return item;
}

function renderDoctorReadList() {
  if (!elements.doctorReadList) return;
  elements.doctorReadList.innerHTML = "";

  const filterValue = elements.doctorFilterRead ? elements.doctorFilterRead.value : "";
  const filtered = filterValue
    ? state.doctors.filter(doc => String(doc.category_id) === filterValue)
    : state.doctors.slice();

  if (filtered.length === 0) {
    elements.doctorReadList.innerHTML = `<div class="empty-state">No doctors found for the selected category.</div>`;
    return;
  }

  filtered.forEach(doc => {
    elements.doctorReadList.appendChild(createDoctorItem(doc, { showDelete: false }));
  });
}

function renderDoctorDeleteList() {
  if (!elements.doctorDeleteList) return;
  elements.doctorDeleteList.innerHTML = "";

  const filterValue = elements.doctorFilterDelete ? elements.doctorFilterDelete.value : "";
  const filtered = filterValue
    ? state.doctors.filter(doc => String(doc.category_id) === filterValue)
    : state.doctors.slice();

  if (filtered.length === 0) {
    elements.doctorDeleteList.innerHTML = `<div class="empty-state">No doctors found for the selected category.</div>`;
    return;
  }

  filtered.forEach(doc => {
    elements.doctorDeleteList.appendChild(createDoctorItem(doc, { showDelete: true }));
  });
}

function renderCategorySelects() {
  const categoryOptions = [
    { value: "", label: "Select a category" },
    ...state.categories.map(cat => ({
      value: String(cat.category_id),
      label: `${cat.category_name} (#${cat.category_id})`
    }))
  ];

  fillSelect(elements.doctorCategorySelect, categoryOptions);
  fillSelect(elements.editCategorySelect, categoryOptions);
  fillSelect(elements.editDoctorCategorySelect, categoryOptions);

  const filterOptions = [
    { value: "", label: "All categories" },
    ...state.categories.map(cat => ({
      value: String(cat.category_id),
      label: cat.category_name
    }))
  ];

  fillSelect(elements.doctorFilterRead, filterOptions);
  fillSelect(elements.doctorFilterDelete, filterOptions);
}

function renderDoctorSelects() {
  const doctorOptions = [
    { value: "", label: "Select a doctor" },
    ...state.doctors.map(doc => ({
      value: String(doc.doctor_id),
      label: `${doc.doctor_name} (#${doc.doctor_id})`
    }))
  ];

  fillSelect(elements.editDoctorSelect, doctorOptions);
}

function syncEditCategoryFields() {
  if (!elements.editCategorySelect) return;
  const id = Number(elements.editCategorySelect.value);
  const category = getCategoryById(id);

  if (!category) {
    if (elements.editCategoryName) elements.editCategoryName.value = "";
    if (elements.editCategoryDescription) elements.editCategoryDescription.value = "";
    return;
  }

  if (elements.editCategoryName) elements.editCategoryName.value = category.category_name || "";
  if (elements.editCategoryDescription) elements.editCategoryDescription.value = category.description || "";
}

function syncEditDoctorFields() {
  if (!elements.editDoctorSelect) return;
  const id = Number(elements.editDoctorSelect.value);
  const doctor = getDoctorById(id);

  if (!doctor) {
    if (elements.editDoctorName) elements.editDoctorName.value = "";
    if (elements.editDoctorPhone) elements.editDoctorPhone.value = "";
    if (elements.editDoctorExperience) elements.editDoctorExperience.value = "";
    if (elements.editDoctorQualifications) elements.editDoctorQualifications.value = "";
    if (elements.editDoctorRatings) elements.editDoctorRatings.value = "";
    if (elements.editDoctorCategorySelect) elements.editDoctorCategorySelect.value = "";
    return;
  }

  if (elements.editDoctorName) elements.editDoctorName.value = doctor.doctor_name || "";
  if (elements.editDoctorPhone) elements.editDoctorPhone.value = doctor.phone || "";
  if (elements.editDoctorExperience) elements.editDoctorExperience.value = doctor.experience_years ?? "";
  if (elements.editDoctorQualifications) elements.editDoctorQualifications.value = doctor.qualifications || "";
  if (elements.editDoctorRatings) elements.editDoctorRatings.value = doctor.ratings ?? "";
  if (elements.editDoctorCategorySelect) elements.editDoctorCategorySelect.value = String(doctor.category_id || "");
}

async function loadCategories() {
  const res = await fetch("/get_categories");
  if (!res.ok) {
    throw new Error("Sorry, we could not load the categories. Please try again.");
  }
  state.categories = await res.json();
}

async function loadDoctors() {
  const res = await fetch("/get_doctors");
  if (!res.ok) {
    throw new Error("Sorry, we could not load the doctors. Please try again.");
  }
  state.doctors = await res.json();
}

async function refreshData() {
  await ensurePublicSession();
  await Promise.all([loadCategories(), loadDoctors()]);
  renderCategorySelects();
  renderDoctorSelects();
  renderCategoryBox();
  renderCategoryReadList();
  renderCategoryDeleteList();
  renderDoctorReadList();
  renderDoctorDeleteList();
  syncEditCategoryFields();
  syncEditDoctorFields();
}

function wireEvents() {
  if (elements.doctorFilterRead) {
    elements.doctorFilterRead.addEventListener("change", renderDoctorReadList);
  }

  if (elements.doctorFilterDelete) {
    elements.doctorFilterDelete.addEventListener("change", renderDoctorDeleteList);
  }

  if (elements.editCategorySelect) {
    elements.editCategorySelect.addEventListener("change", syncEditCategoryFields);
  }

  if (elements.editDoctorSelect) {
    elements.editDoctorSelect.addEventListener("change", syncEditDoctorFields);
  }

  if (elements.addCategoryForm) {
    elements.addCategoryForm.addEventListener("submit", async event => {
      event.preventDefault();

      const name = document.getElementById("category_name").value.trim();
      const description = document.getElementById("description").value.trim();

      if (!safePattern.test(name)) {
        showToast("Please use letters and numbers only for the category name.", "error");
        return;
      }

      if (!safePattern.test(description.replace(/[\.,!?\'\"]/g, ""))) {
        showToast("Please use letters and numbers only in the description.", "error");
        return;
      }

      const res = await fetch("/add_category", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ category_name: name, description })
      });

      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Category added successfully.");
        elements.addCategoryForm.reset();
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not add that category. Please try again.", "error");
      }
    });
  }

  if (elements.addDoctorForm) {
    elements.addDoctorForm.addEventListener("submit", async event => {
      event.preventDefault();

      const doctorName = document.getElementById("doctor_name").value.trim();
      const phone = document.getElementById("phone").value.trim();
      const experienceYears = document.getElementById("experience_years").value.trim();
      const qualifications = document.getElementById("qualifications").value.trim();
      const ratings = document.getElementById("ratings").value.trim();
      const categoryId = document.getElementById("doctor_category_id").value;

      if (!safeDoctorPattern.test(doctorName)) {
        showToast("Please use letters, numbers, and punctuation only for the doctor name.", "error");
        return;
      }

      if (!/^[0-9+\-\s]{7,20}$/.test(phone)) {
        showToast("Please enter a valid phone number.", "error");
        return;
      }

      if (!safeDoctorPattern.test(qualifications)) {
        showToast("Please use letters, numbers, and punctuation only for qualifications.", "error");
        return;
      }

      if (!categoryId) {
        showToast("Please select a category before adding a doctor.", "error");
        return;
      }

      const res = await fetch("/add_doctor", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          doctor_name: doctorName,
          phone,
          experience_years: experienceYears,
          qualifications,
          ratings: Number(ratings),
          category_id: Number(categoryId)
        })
      });

      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Doctor added successfully.");
        elements.addDoctorForm.reset();
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not add that doctor. Please try again.", "error");
      }
    });
  }

  if (elements.editCategoryForm) {
    elements.editCategoryForm.addEventListener("submit", async event => {
      event.preventDefault();

      const categoryId = elements.editCategorySelect ? elements.editCategorySelect.value : "";
      const name = elements.editCategoryName ? elements.editCategoryName.value.trim() : "";
      const description = elements.editCategoryDescription ? elements.editCategoryDescription.value.trim() : "";

      if (!categoryId) {
        showToast("Please select a category to update.", "error");
        return;
      }

      if (!safePattern.test(name)) {
        showToast("Please use letters and numbers only for the category name.", "error");
        return;
      }

      if (!safePattern.test(description.replace(/[\.,!?\'\"]/g, ""))) {
        showToast("Please use letters and numbers only in the description.", "error");
        return;
      }

      const res = await fetch(`/update_category/${categoryId}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ category_name: name, description })
      });

      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Category updated successfully.");
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not update that category. Please try again.", "error");
      }
    });
  }

  if (elements.editDoctorForm) {
    elements.editDoctorForm.addEventListener("submit", async event => {
      event.preventDefault();

      const doctorId = elements.editDoctorSelect ? elements.editDoctorSelect.value : "";
      const doctorName = elements.editDoctorName ? elements.editDoctorName.value.trim() : "";
      const phone = elements.editDoctorPhone ? elements.editDoctorPhone.value.trim() : "";
      const experienceYears = elements.editDoctorExperience ? elements.editDoctorExperience.value.trim() : "";
      const qualifications = elements.editDoctorQualifications ? elements.editDoctorQualifications.value.trim() : "";
      const ratings = elements.editDoctorRatings ? elements.editDoctorRatings.value.trim() : "";
      const categoryId = elements.editDoctorCategorySelect ? elements.editDoctorCategorySelect.value : "";

      if (!doctorId) {
        showToast("Please select a doctor to update.", "error");
        return;
      }

      if (!safeDoctorPattern.test(doctorName)) {
        showToast("Please use letters, numbers, and punctuation only for the doctor name.", "error");
        return;
      }

      if (!/^[0-9+\-\s]{7,20}$/.test(phone)) {
        showToast("Please enter a valid phone number.", "error");
        return;
      }

      if (!safeDoctorPattern.test(qualifications)) {
        showToast("Please use letters, numbers, and punctuation only for qualifications.", "error");
        return;
      }

      if (!categoryId) {
        showToast("Please select a category before updating the doctor.", "error");
        return;
      }

      const res = await fetch(`/update_doctor/${doctorId}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          doctor_name: doctorName,
          phone,
          experience_years: experienceYears,
          qualifications,
          ratings: Number(ratings),
          category_id: Number(categoryId)
        })
      });

      const data = await res.json().catch(() => ({}));
      if (res.ok && data.success) {
        showToast("Doctor updated successfully.");
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not update that doctor. Please try again.", "error");
      }
    });
  }

  const cards = document.querySelectorAll(".crud-card");
  cards.forEach(card => {
    card.addEventListener("toggle", () => {
      if (card.open) {
        cards.forEach(other => {
          if (other !== card) other.open = false;
        });
      }
    });
  });

  document.querySelectorAll(".admin-nav a").forEach(link => {
    link.addEventListener("click", () => {
      const targetId = link.getAttribute("href")?.replace("#", "");
      const target = targetId ? document.getElementById(targetId) : null;
      if (target && target.tagName === "DETAILS") {
        target.open = true;
      }
    });
  });
}

async function init() {
  try {
    await refreshData();
  } catch (error) {
    console.error(error);
    showToast("Sorry, we could not load the admin panel. Please refresh and try again.", "error");
  }

  wireEvents();
}

init();
