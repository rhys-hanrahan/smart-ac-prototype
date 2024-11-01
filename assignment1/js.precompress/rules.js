document.addEventListener('DOMContentLoaded', async () => {
    const token = localStorage.getItem('jwtToken');
    if (!token) {
        window.location.href = '/login';
        return;
    }

    // Token check
    const authCheck = await fetch('/api/auth-check', {
        headers: { 'Authorization': `Bearer ${token}` }
    });
    if (!authCheck.ok) {
        localStorage.removeItem('jwtToken');
        window.location.href = '/login';
        return;
    }

    // Fetch and display rules
    await loadRules();

    // Save rules on button click
    document.getElementById('save-rules').addEventListener('click', async () => {
        await saveRules();
    });

    // Add a new rule card
    document.getElementById('add-rule').addEventListener('click', () => {
        const newRule = createRuleHtml({}, document.getElementById('rule-list').children.length);
        document.getElementById('rule-list').insertAdjacentHTML('beforeend', newRule);
    });
});

// Fetch rules from API and populate form
async function loadRules() {
    try {
        const response = await fetch('/api/rules', {
            headers: { 'Authorization': `Bearer ${localStorage.getItem('jwtToken')}` }
        });

        if (response.ok) {
            const rules = await response.json();
            const ruleList = document.getElementById('rule-list');
            ruleList.innerHTML = ''; // Clear existing rules

            rules.forEach((rule, index) => {
                ruleList.insertAdjacentHTML('beforeend', createRuleHtml(rule, index));
            });
        }
    } catch (error) {
        console.error('Failed to load rules:', error);
    }
}

// Generate HTML for a rule card with add/remove controls for conditions/actions
function createRuleHtml(rule = {}, index) {
    return `
        <div class="card mb-3" id="rule-${index}">
            <div class="card-body">
                <h5 class="card-title">Rule: <input type="text" value="${rule.name || ''}" id="rule-${index}-name"></h5>
                <textarea placeholder="Description" class="form-control mb-2" id="rule-${index}-description">${rule.description || ''}</textarea>
                
                <h6>Timeframe</h6>
                <label>Days (comma-separated)</label>
                <input type="text" class="form-control mb-2" id="rule-${index}-days" value="${(rule.timeframe?.days || []).join(', ')}">
                
                <label>Start Time</label>
                <input type="time" class="form-control mb-2" id="rule-${index}-start-time" value="${rule.timeframe?.start_time || ''}">
                
                <label>End Time</label>
                <input type="time" class="form-control mb-2" id="rule-${index}-end-time" value="${rule.timeframe?.end_time || ''}">
                
                <label>Seasons (comma-separated)</label>
                <input type="text" class="form-control mb-2" id="rule-${index}-seasons" value="${(rule.timeframe?.seasons || []).join(', ')}">
                
                <h6>Conditions</h6>
                <div id="conditions-${index}">
                    ${(rule.conditions?.conditions || []).map((cond, condIdx) => createConditionHtml(index, condIdx, cond)).join('')}
                </div>
                <button class="btn btn-sm btn-secondary" onclick="addCondition(${index})">Add Condition</button>
                
                <h6>Actions</h6>
                <div id="actions-${index}">
                    ${(rule.actions || []).map((action, actIdx) => createActionHtml(index, actIdx, action)).join('')}
                </div>
                <button class="btn btn-sm btn-secondary" onclick="addAction(${index})">Add Action</button>
            </div>
        </div>
    `;
}

// Generate HTML for a condition row
function createConditionHtml(ruleIndex, condIndex, condition = {}) {
    return `
        <div class="form-group" id="rule-${ruleIndex}-condition-${condIndex}">
            <label>Field</label>
            <input type="text" class="form-control mb-1" placeholder="Field" value="${condition.field || ''}" id="rule-${ruleIndex}-condition-${condIndex}-field">
            <label>Operator</label>
            <input type="text" class="form-control mb-1" placeholder="Operator" value="${condition.operator_ || ''}" id="rule-${ruleIndex}-condition-${condIndex}-operator">
            <label>Value</label>
            <input type="number" class="form-control mb-1" placeholder="Value" value="${condition.value || ''}" id="rule-${ruleIndex}-condition-${condIndex}-value">
            <button class="btn btn-sm btn-danger mt-2" onclick="removeCondition(${ruleIndex}, ${condIndex})">Remove Condition</button>
        </div>
    `;
}

// Generate HTML for an action row
function createActionHtml(ruleIndex, actIndex, action = {}) {
    return `
        <div class="form-group" id="rule-${ruleIndex}-action-${actIndex}">
            <label>Action Type</label>
            <input type="text" class="form-control mb-1" placeholder="Type" value="${action.type || ''}" id="rule-${ruleIndex}-action-${actIndex}-type">
            <label>Target Temp</label>
            <input type="number" class="form-control mb-1" placeholder="Target Temp" value="${action.target_temp || ''}" id="rule-${ruleIndex}-action-${actIndex}-target-temp">
            <label>Increment Value</label>
            <input type="number" class="form-control mb-1" placeholder="Increment Value" value="${action.increment_value || ''}" id="rule-${ruleIndex}-action-${actIndex}-increment-value">
            <button class="btn btn-sm btn-danger mt-2" onclick="removeAction(${ruleIndex}, ${actIndex})">Remove Action</button>
        </div>
    `;
}

// Functions to add or remove conditions and actions
function addCondition(ruleIndex) {
    const conditionContainer = document.getElementById(`conditions-${ruleIndex}`);
    const condIndex = conditionContainer.children.length;
    conditionContainer.insertAdjacentHTML('beforeend', createConditionHtml(ruleIndex, condIndex));
}

function removeCondition(ruleIndex, condIndex) {
    document.getElementById(`rule-${ruleIndex}-condition-${condIndex}`).remove();
}

function addAction(ruleIndex) {
    const actionContainer = document.getElementById(`actions-${ruleIndex}`);
    const actIndex = actionContainer.children.length;
    actionContainer.insertAdjacentHTML('beforeend', createActionHtml(ruleIndex, actIndex));
}

function removeAction(ruleIndex, actIndex) {
    document.getElementById(`rule-${ruleIndex}-action-${actIndex}`).remove();
}

// Collect rule data from the form and save it via API
async function saveRules() {
    const rulesData = Array.from(document.querySelectorAll('.card')).map((card, index) => {
        return {
            name: card.querySelector(`#rule-${index}-name`).value,
            description: card.querySelector(`#rule-${index}-description`).value,
            timeframe: {
                days: card.querySelector(`#rule-${index}-days`).value.split(',').map(d => d.trim()),
                start_time: card.querySelector(`#rule-${index}-start-time`).value,
                end_time: card.querySelector(`#rule-${index}-end-time`).value,
                seasons: card.querySelector(`#rule-${index}-seasons`).value.split(',').map(s => s.trim())
            },
            conditions: {
                operator: 'AND',
                conditions: Array.from(card.querySelectorAll(`#conditions-${index} .form-group`)).map((_, condIdx) => ({
                    field: card.querySelector(`#rule-${index}-condition-${condIdx}-field`).value,
                    operator_: card.querySelector(`#rule-${index}-condition-${condIdx}-operator`).value,
                    value: parseFloat(card.querySelector(`#rule-${index}-condition-${condIdx}-value`).value)
                }))
            },
            actions: Array.from(card.querySelectorAll(`#actions-${index} .form-group`)).map((_, actIdx) => ({
                type: card.querySelector(`#rule-${index}-action-${actIdx}-type`).value,
                target_temp: parseFloat(card.querySelector(`#rule-${index}-action-${actIdx}-target-temp`).value),
                increment_value: parseFloat(card.querySelector(`#rule-${index}-action-${actIdx}-increment-value`).value)
            }))
        };
    });

    try {
        const response = await fetch('/api/rules', {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${localStorage.getItem('jwtToken')}`,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ rules: rulesData })
        });
        
        if (response.ok) alert('Rules saved successfully!');
        else console.error('Failed to save rules');
    } catch (error) {
        console.error('Error saving rules:', error);
    }
}