const state = {
  categories: [],
  doctors: [],
  selectedCategoryId: null
};

const elements = {
  categoryList: document.getElementById("categoryList"),
  categoryBox: document.getElementById("categoryBox"),
  addCategoryForm: document.getElementById("addCategoryForm"),
  addDoctorForm: document.getElementById("addDoctorForm"),
  doctorList: document.getElementById("doctorList"),
  doctorCategorySelect: document.getElementById("doctor_category_id"),
  doctorFilter: document.getElementById("doctorFilter"),
  selectedCategoryChip: document.getElementById("selectedCategoryChip"),
  toast: document.getElementById("toast")
};

const safePattern = /^[a-zA-Z0-9\s]+$/;
const safeDoctorPattern = /^[a-zA-Z0-9\s\.,-]+$/;

function showToast(message, type = "success") {
  elements.toast.textContent = message;
  elements.toast.className = `toast show ${type}`;
  setTimeout(() => {
    elements.toast.className = "toast";
  }, 3000);
}

function getCategoryById(id) {
  return state.categories.find(cat => cat.category_id === id) || null;
}

function renderCategoryBox() {
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

function renderCategoryList() {
  elements.categoryList.innerHTML = "";

  if (state.categories.length === 0) {
    elements.categoryList.innerHTML = `<div class="empty-state">No categories yet. Please add one.</div>`;
    return;
  }

  state.categories.forEach(category => {
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

    const selectBtn = document.createElement("button");
    selectBtn.type = "button";
    selectBtn.className = "btn btn-outline";
    selectBtn.textContent = "Select";
    selectBtn.addEventListener("click", () => {
      state.selectedCategoryId = category.category_id;
      elements.doctorCategorySelect.value = String(category.category_id);
      elements.doctorFilter.value = String(category.category_id);
      renderCategoryBox();
      renderDoctorList();
    });

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
        await refreshData();
      } else {
        showToast(data.message || "Sorry, we could not delete that category. Please try again.", "error");
      }
    });

    actionRow.appendChild(selectBtn);
    actionRow.appendChild(deleteBtn);
    item.appendChild(actionRow);
    elements.categoryList.appendChild(item);
  });
}

function renderCategorySelects() {
  const options = [
    { value: "", label: "Select a category" },
    ...state.categories.map(cat => ({
      value: String(cat.category_id),
      label: `${cat.category_name} (#${cat.category_id})`
    }))
  ];

  elements.doctorCategorySelect.innerHTML = "";
  options.forEach(option => {
    const el = document.createElement("option");
    el.value = option.value;
    el.textContent = option.label;
    elements.doctorCategorySelect.appendChild(el);
  });

  elements.doctorFilter.innerHTML = "";
  const filterOptions = [
    { value: "", label: "All categories" },
    ...state.categories.map(cat => ({
      value: String(cat.category_id),
      label: cat.category_name
    }))
  ];

  filterOptions.forEach(option => {
    const el = document.createElement("option");
    el.value = option.value;
    el.textContent = option.label;
    elements.doctorFilter.appendChild(el);
  });
}

function renderDoctorList() {
  elements.doctorList.innerHTML = "";

  const filterValue = elements.doctorFilter.value;
  const filtered = filterValue
    ? state.doctors.filter(doc => String(doc.category_id) === filterValue)
    : state.doctors.slice();

  if (filtered.length === 0) {
    elements.doctorList.innerHTML = `<div class="empty-state">No doctors found for the selected category.</div>`;
    return;
  }

  filtered.forEach(doc => {
    const item = document.createElement("div");
    item.className = "list-item";

    const category = getCategoryById(doc.category_id);
    const categoryName = category ? category.category_name : `Category #${doc.category_id}`;

    item.innerHTML = `
      <div class="meta">
        <strong>${doc.doctor_name}</strong>
        <span>${categoryName} · ${doc.experience_years || 0} yrs · ${doc.qualifications || "No qualification"}</span>
        <span>Rating: ${doc.ratings ?? "N/A"}</span>
      </div>
    `;

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
    elements.doctorList.appendChild(item);
  });
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
  await Promise.all([loadCategories(), loadDoctors()]);
  renderCategorySelects();
  renderCategoryList();
  renderCategoryBox();
  renderDoctorList();
}

function wireEvents() {
  elements.doctorFilter.addEventListener("change", () => {
    const selected = elements.doctorFilter.value;
    state.selectedCategoryId = selected ? Number(selected) : null;
    elements.doctorCategorySelect.value = selected;
    renderCategoryBox();
    renderDoctorList();
  });

  elements.addCategoryForm.addEventListener("submit", async event => {
    event.preventDefault();

    const name = document.getElementById("category_name").value.trim();
    const description = document.getElementById("description").value.trim();

    if (!safePattern.test(name)) {
      showToast("Please use letters and numbers only for the category name.", "error");
      return;
    }

    if (!safePattern.test(description.replace(/[\.,!?'\"]/g, ""))) {
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
