// Performance Report Interactive Dashboard JavaScript

const data                 = __{data};
const paramNames           = __{paramNames};
const metricNames          = __{metricNames};
const defaultVisibleMetric = __{defaultVisibleMetric};
const uniqueMarkers        = __{uniqueMarkers};

let selections       = {};
let metricVisibility = {};
let currentTab       = uniqueMarkers.length > 0 ? uniqueMarkers[0] : '';

// Initialize selections and visibility
paramNames.forEach(param => selections[param] = '');
metricNames.forEach(metric => {
    metricVisibility[metric] = (metric === defaultVisibleMetric) ? true : 'legendonly';
});

// Create tabs
function createTabs()
{
    const tabsDiv = document.getElementById('tabs');

    // Create tabs for each marker (no "All" tab)
    uniqueMarkers.forEach((marker, index) => {
        const tabBtn       = document.createElement('button');
        tabBtn.className   = index === 0 ? 'tab-button active' : 'tab-button';
        tabBtn.textContent = marker;
        tabBtn.onclick = () => switchTab(marker);
        tabsDiv.appendChild(tabBtn);
    });

    // Set the first marker as default
    if (uniqueMarkers.length > 0)
    {
        currentTab = uniqueMarkers[0];
    }
}

// Switch between tabs
function switchTab(tabId)
{
    currentTab = tabId;

    // Update tab button styles
    document.querySelectorAll('.tab-button').forEach(btn => {
        btn.classList.remove('active');
        if (btn.textContent === tabId)
        {
            btn.classList.add('active');
        }
    });

    // Update plot based on current tab
    updatePlot();
}

// Create controls
function createControls()
{
    const controlsDiv = document.getElementById('controls');

    // Create dropdowns for each parameter
    paramNames.forEach(param => {
        const label       = document.createElement('label');
        label.textContent = param + ': ';
        const select      = document.createElement('select');
        select.id         = param;
        select.onchange = () => {
            selections[param] = select.value;
            updateDropdownOptions();
            updatePlot();
        };
        label.appendChild(select);
        controlsDiv.appendChild(label);
    });

    // Add reset all filters button
    const resetBtn       = document.createElement('button');
    resetBtn.textContent = 'Reset All Filters';
    resetBtn.onclick = () => {
        // Reset all selections
        paramNames.forEach(param => {
            selections[param]                    = '';
            document.getElementById(param).value = '';
        });
        updateDropdownOptions();
        updatePlot();
    };
    controlsDiv.appendChild(resetBtn);

    updateDropdownOptions();
}

// Update dropdown options based on current selections
function updateDropdownOptions()
{
    // Filter data based on current selections
    const filteredData = data.filter(record => Object.entries(selections).every(([param, value]) => !value || record[param] === value));

    // Update each dropdown with available values
    paramNames.forEach(param => {
        const select          = document.getElementById(param);
        const currentValue    = select.value;
        const availableValues = [...new Set(filteredData.map(record => record[param]))];

        // Rebuild options
        select.innerHTML = '<option value="">All</option>' + availableValues
                                                                 .map(value => {
                                                                     const selected = value === currentValue ? 'selected' : '';
                                                                     return `<option value="${value}" ${selected}>${value}</option>`;
                                                                 })
                                                                 .join('');

        // Update selection
        selections[param] = select.value;
    });
}

// Create the plot
function createPlot()
{
    const plotDiv = document.getElementById('plot');

    // Filter data for the current tab from the start
    let initialData = data.filter(record => record.marker === currentTab);

    // Create initial traces for all metrics
    const traces = metricNames.map(metric => {
        const hoverText = initialData.map(record => paramNames.map(param => `${param}=${record[param]}`).join(', '));

        return {
            x: initialData.map((record, index) => index + 1),
            y: initialData.map(record => record[metric]),
            type: 'bar',
            name: metric,
            hovertemplate: `<b>%{customdata}</b><br>${metric}: %{y}<extra></extra>`,
            customdata: hoverText,
            visible: metricVisibility[metric]
        };
    });

    const layout = {
        title: {text: `Performance Report - ${currentTab}`, font: {size: 18}, x: 0.5, xanchor: 'center'},
        xaxis: {
            title: {text: 'Sweep Index (hover for parameter details)', font: {size: 14, color: '#333'}},
            showgrid: true,
            gridcolor: '#e0e0e0',
            tickfont: {size: 12, color: '#333'}
        },
        yaxis: {title: {text: 'Cycles / Tile', font: {size: 14, color: '#333'}}, showgrid: true, gridcolor: '#e0e0e0', tickfont: {size: 12, color: '#333'}},
        legend: {title: {text: 'Metrics', font: {size: 12}}, orientation: 'v', x: 1.02, xanchor: 'left', y: 1, yanchor: 'top'},
        template: 'plotly_white',
        margin: {l: 60, r: 120, t: 50, b: 60},
        autosize: true
    };

    Plotly.newPlot(plotDiv, traces, layout, {responsive: true, displayModeBar: true});

    // Handle legend clicks to toggle metric visibility
    plotDiv.on('plotly_legendclick', function(eventData) {
        const metricName             = eventData.data[eventData.curveNumber].name;
        const currentVisibility      = metricVisibility[metricName];
        metricVisibility[metricName] = (currentVisibility === true) ? 'legendonly' : true;
        updatePlot();
        return false; // Prevent default legend click behavior
    });

    // Resize plot when window resizes
    window.addEventListener('resize', function() {
        Plotly.Plots.resize(plotDiv);
    });
}

// Update the plot based on current filters and settings
function updatePlot()
{
    const plotDiv = document.getElementById('plot');

    // Apply filters
    paramNames.forEach(param => {
        const select      = document.getElementById(param);
        selections[param] = select.value;
    });

    // Filter data based on parameter selections
    let filteredData = data.filter(record => Object.entries(selections).every(([param, value]) => !value || record[param] === value));

    // Filter by current tab (marker)
    filteredData = filteredData.filter(record => record.marker === currentTab);

    // Handle empty data case
    if (filteredData.length === 0)
    {
        const layout = {
            title: {text: `Performance Report - ${currentTab}`, font: {size: 18}, x: 0.5, xanchor: 'center'},
            xaxis: {title: 'No data available for current filters'},
            yaxis: {title: 'Cycles / Tile'}
        };
        Plotly.react(plotDiv, [], layout);
        return;
    }

    const indices = filteredData.map((record, index) => index + 1);

    // Create traces for visible metrics
    const traces = metricNames.map(metric => {
        const visible   = metricVisibility[metric];
        const yValues   = filteredData.map(record => record[metric]);
        const hoverText = filteredData.map(record => paramNames.map(param => `${param}=${record[param]}`).join(', '));

        return {
            x: indices,
            y: yValues,
            type: 'bar',
            name: metric,
            visible: visible,
            customdata: hoverText,
            hovertemplate: `<b>%{customdata}</b><br>${metric}: %{y}<extra></extra>`
        };
    });

    const layout = {
        title: {text: `Performance Report - ${currentTab}`, font: {size: 18}, x: 0.5, xanchor: 'center'},
        xaxis: {
            title: {text: 'Sweep Index (hover for parameter details)', font: {size: 14, color: '#333'}},
            showgrid: true,
            gridcolor: '#e0e0e0',
            tickfont: {size: 12, color: '#333'}
        },
        yaxis: {title: {text: 'Cycles / Tile', font: {size: 14, color: '#333'}}, showgrid: true, gridcolor: '#e0e0e0', tickfont: {size: 12, color: '#333'}},
        legend: {title: {text: 'Metrics', font: {size: 12}}, orientation: 'v', x: 1.02, xanchor: 'left', y: 1, yanchor: 'top'},
        template: 'plotly_white',
        margin: {l: 60, r: 120, t: 50, b: 60},
        autosize: true
    };

    Plotly.react(plotDiv, traces, layout);
}

// Initialize everything
createTabs();
createControls();
createPlot();
