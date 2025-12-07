// =============================================
// GERENCIAMENTO DE TOKEN
// =============================================
class StorageManager {
    static setToken(token) { localStorage.setItem('agricultureSystem_token', token); }
    static getToken() { return localStorage.getItem('agricultureSystem_token'); }
    static clearToken() { localStorage.removeItem('agricultureSystem_token'); }
}

// =============================================
// NOTIFICAÇÕES
// =============================================
class ErrorHandler {
    static showNotification(message, type = 'error') {
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.textContent = message;
        document.body.appendChild(notification);
        setTimeout(() => notification.classList.add('show'), 100);
        setTimeout(() => { notification.classList.remove('show'); notification.remove(); }, 5000);
    }
}

// =============================================
// APLICAÇÃO PRINCIPAL
// =============================================
class AgricultureApp {
    constructor() {
        this.API_KEY = 'c078160b604897141ff65b50363e12a8'; 
        this.BASE_URL = 'https://api.openweathermap.org/data/2.5';
        this.API_BACKEND = '/api'; 
        
        this.currentTemp = 25.0;
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.checkSession();
        const dateInput = document.getElementById('planting-date');
        if(dateInput) dateInput.valueAsDate = new Date();
    }

    setupEventListeners() {
        document.getElementById('switch-to-register').addEventListener('click', (e) => { e.preventDefault(); this.showRegisterForm(); });
        document.getElementById('switch-to-login').addEventListener('click', (e) => { e.preventDefault(); this.showLoginForm(); });
        document.getElementById('login-form').addEventListener('submit', (e) => this.handleLogin(e));
        document.getElementById('register-form').addEventListener('submit', (e) => this.handleRegister(e));
        document.getElementById('logout-btn').addEventListener('click', () => this.logoutUser());
        document.getElementById('crop-form').addEventListener('submit', (e) => this.handleCropSubmit(e));

        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', () => this.switchTab(tab));
        });

        document.querySelector('.search-btn').addEventListener('click', () => this.handleSearch());
    }

    showLoginForm() {
        document.getElementById('login-form').style.display = 'block';
        document.getElementById('register-form').style.display = 'none';
    }

    showRegisterForm() {
        document.getElementById('login-form').style.display = 'none';
        document.getElementById('register-form').style.display = 'block';
    }

    switchTab(tab) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        tab.classList.add('active');
        document.getElementById(`${tab.dataset.tab}-content`).classList.add('active');
    }

    // ================= INTEGRAÇÃO COM C++ =================

    async handleRegister(e) {
        e.preventDefault();
        const name = document.getElementById('register-name').value;
        const email = document.getElementById('register-email').value;
        const password = document.getElementById('register-password').value;

        try {
            const res = await fetch(`${this.API_BACKEND}/register`, {
                method: 'POST',
                body: JSON.stringify({ name, email, password })
            });
            const json = await res.json();

            if (json.success === "true") {
                ErrorHandler.showNotification('Cadastro realizado!', 'success');
                this.showLoginForm();
            } else {
                ErrorHandler.showNotification(json.message || 'Erro ao cadastrar', 'error');
            }
        } catch (err) {
            console.error(err);
            ErrorHandler.showNotification('Erro de conexão com servidor C++', 'error');
        }
    }

    async handleLogin(e) {
        e.preventDefault();
        const email = document.getElementById('login-email').value;
        const password = document.getElementById('login-password').value;

        try {
            const res = await fetch(`${this.API_BACKEND}/login`, {
                method: 'POST',
                body: JSON.stringify({ email, password })
            });
            const json = await res.json();

            if (json.success === "true") {
                StorageManager.setToken(json.token);
                const user = { name: json.user_name, email: json.user_email };
                localStorage.setItem('user_data', JSON.stringify(user));
                this.loginUser(user);
            } else {
                ErrorHandler.showNotification(json.message || 'Login inválido', 'error');
            }
        } catch (err) {
            console.error(err);
            ErrorHandler.showNotification('Erro de conexão com servidor C++', 'error');
        }
    }

    loginUser(user) {
        document.getElementById('auth-container').style.display = 'none';
        document.getElementById('app-content').style.display = 'block';
        document.getElementById('user-greeting').textContent = `Olá, ${user.name}!`;
        document.getElementById('user-email').textContent = user.email;
        this.handleSearch();
        this.loadCrops();
    }

    logoutUser() {
        StorageManager.clearToken();
        localStorage.removeItem('user_data');
        window.location.reload();
    }

    checkSession() {
        const token = StorageManager.getToken();
        if (token) {
            const user = JSON.parse(localStorage.getItem('user_data') || '{}');
            if (user.name) this.loginUser(user);
        }
    }

    // --- CULTURAS (OPENCL & BACKEND) ---

    async loadCrops() {
        const container = document.getElementById('crops-container');
        container.innerHTML = 'Carregando...';

        try {
            const res = await fetch(`${this.API_BACKEND}/crops`);
            const json = await res.json();

            container.innerHTML = '';
            if (json.crops && json.crops.length > 0) {
                json.crops.forEach(crop => {
                    const div = document.createElement('div');
                    div.className = 'crop-item';
                    div.innerHTML = `
                        <div class="crop-header">
                            <span class="crop-name">${crop.name}</span>
                            <span class="crop-date">${crop.plantingDate}</span>
                        </div>
                        <div class="crop-details">
                            <div class="crop-detail">Tipo: ${crop.type}</div>
                            <div class="crop-detail">Área: ${parseFloat(crop.area)} m²</div>
                        </div>
                        <div style="margin-top:10px; display:flex; gap:5px;">
                            <button onclick="window.app.deleteCrop(${crop.id})" class="btn" style="background:var(--danger-color);color:white;font-size:0.8rem;padding:5px 10px;">Excluir</button>
                            <button onclick="window.app.runSimulation(${crop.area})" class="btn" style="background:var(--primary-color);color:white;font-size:0.8rem;padding:5px 10px;">
                                <i class="fas fa-microchip"></i> Simular Colheita (OpenCL)
                            </button>
                        </div>
                    `;
                    container.appendChild(div);
                });
            } else {
                container.innerHTML = '<p>Nenhuma cultura cadastrada.</p>';
            }
        } catch (err) {
            console.error(err);
            container.innerHTML = '<p style="color:red">Erro ao carregar culturas.</p>';
        }
    }

    async runSimulation(area) {
        ErrorHandler.showNotification('Enviando para GPU (OpenCL)...', 'warning');
        try {
            const res = await fetch(`${this.API_BACKEND}/simulation`, {
                method: 'POST',
                body: JSON.stringify({ area: area.toString(), temp: this.currentTemp.toString() })
            });
            const json = await res.json();
            if (json.success === "true") {
                alert(`🔮 SIMULAÇÃO OPENCL CONCLUÍDA 🔮\n\nProdutividade Estimada: ${parseFloat(json.producao_estimada).toFixed(2)} kg\n\nCálculo realizado via Kernel C++`);
            } else {
                ErrorHandler.showNotification('Erro no backend.', 'error');
            }
        } catch (err) { 
            ErrorHandler.showNotification('Erro na simulação', 'error'); 
        }
    }

    async handleCropSubmit(e) {
        e.preventDefault();
        const data = {
            name: document.getElementById('crop-name').value,
            type: document.getElementById('crop-type').value,
            plantingDate: document.getElementById('planting-date').value,
            area: document.getElementById('crop-area').value
        };

        try {
            await fetch(`${this.API_BACKEND}/crops`, {
                method: 'POST',
                body: JSON.stringify(data)
            });
            document.getElementById('crop-form').reset();
            this.loadCrops();
            ErrorHandler.showNotification('Cultura salva com sucesso!', 'success');
        } catch (err) {
            ErrorHandler.showNotification('Erro ao salvar.', 'error');
        }
    }

    async deleteCrop(id) {
        if(!confirm('Deseja realmente excluir?')) return;
        try {
            await fetch(`${this.API_BACKEND}/crops/${id}`, { method: 'DELETE' });
            this.loadCrops();
            ErrorHandler.showNotification('Removido!', 'success');
        } catch (err) {
            ErrorHandler.showNotification('Erro ao remover.', 'error');
        }
    }

    // ================= CLIMA (OPENWEATHER) =================
    
    async handleSearch() {
        // AQUI ESTÁ A MUDANÇA: Padrão Recife
        const city = document.getElementById('city-search').value || 'Recife';
        
        try {
            document.getElementById('loading').style.display = 'block';
            document.getElementById('weather-content').style.display = 'none';

            // 1. Busca Tempo Atual
            const res = await fetch(`${this.BASE_URL}/weather?q=${city}&units=metric&appid=${this.API_KEY}&lang=pt_br`);
            if(!res.ok) throw new Error('Cidade não encontrada');
            const data = await res.json();
            this.currentTemp = data.main.temp;
            this.updateWeatherUI(data);

            // 2. Busca Previsão
            try {
                const res2 = await fetch(`${this.BASE_URL}/forecast?q=${city}&units=metric&appid=${this.API_KEY}&lang=pt_br`);
                if (res2.ok) {
                    const data2 = await res2.json();
                    this.updateForecastUI(data2);
                } else {
                    document.getElementById('forecast-container').innerHTML = '<p style="text-align:center; color:#666;">Previsão indisponível.</p>';
                }
            } catch (forecastErr) {
                console.warn("Erro ao buscar previsão:", forecastErr);
            }
            
        } catch (err) {
            console.error(err);
            document.getElementById('loading').style.display = 'none';
            document.getElementById('error-message').style.display = 'block';
            ErrorHandler.showNotification(err.message, 'error');
        }
    }

    updateWeatherUI(data) {
        document.querySelector('.temp').textContent = `${Math.round(data.main.temp)}°C`;
        document.querySelector('.weather-text').textContent = data.weather[0].description;
        document.querySelector('.location').innerHTML = `<i class="fas fa-map-marker-alt"></i> ${data.name}`;
        
        const detailValues = document.querySelectorAll('.detail-value');
        if (detailValues.length >= 4) {
            detailValues[0].textContent = `${data.main.humidity}%`;
            detailValues[1].textContent = `${data.wind.speed} km/h`;
            detailValues[2].textContent = `${data.main.pressure} hPa`;
            const visibilityKm = data.visibility ? (data.visibility / 1000).toFixed(1) : '--';
            detailValues[3].textContent = `${visibilityKm} km`;
        }

        document.getElementById('weather-content').style.display = 'block';
        document.getElementById('loading').style.display = 'none';
        document.getElementById('error-message').style.display = 'none';
        
        this.generateRecommendations(data.main.temp, data.main.humidity);
    }

    updateForecastUI(data) {
        const container = document.getElementById('forecast-container');
        container.innerHTML = '';
        
        if (!data || !data.list) return;

        for(let i=0; i < data.list.length; i+=8) {
            const item = data.list[i];
            const div = document.createElement('div');
            div.className = 'forecast-card';
            div.innerHTML = `
                <div class="forecast-day">${new Date(item.dt*1000).toLocaleDateString('pt-BR')}</div>
                <div class="forecast-temp">${Math.round(item.main.temp)}°C</div>
                <div class="forecast-desc">${item.weather[0].description}</div>
            `;
            container.appendChild(div);
        }
    }

    generateRecommendations(temp, humidity) {
        let recCrops = 'Variadas';
        let recIrrigation = 'Padrão (1x ao dia)';
        let recPeriod = 'Todo o dia';
        
        if(temp > 28) {
            recCrops = 'Caju, Cana, Manga (Resistentes)';
            recIrrigation = 'Intensiva (2-3x ao dia)';
            recPeriod = 'Início da manhã ou fim da tarde';
        } else if(temp < 20) {
            recCrops = 'Milho, Feijão, Trigo (Ameno)';
            recIrrigation = 'Moderada (Observar solo)';
            recPeriod = 'Evitar geadas';
        } else {
            recCrops = 'Hortaliças, Frutas (Ideal)';
            recIrrigation = 'Padrão (1x ao dia)';
            recPeriod = 'Qualquer horário';
        }
        
        document.getElementById('recommended-crops').textContent = recCrops;
        document.getElementById('irrigation-tip').textContent = recIrrigation;
        document.getElementById('planting-period').textContent = recPeriod;

        document.getElementById('alert-text').textContent = (temp > 30 || humidity < 40) ? 'ALERTA: Clima Extremo (Calor/Seca)' : 'Condições Favoráveis';
    }
}

window.app = new AgricultureApp();