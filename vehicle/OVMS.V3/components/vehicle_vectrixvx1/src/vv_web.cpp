/**
 * Project:      Open Vehicle Monitor System
 * Module:       Vectrix VX1 Webserver
 *
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER


#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_vectrixvx1.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleVectrixVX1::WebInit()
{
  // vehicle menu:
  //MyWebServer.RegisterPage("/xnl/features", "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  //MyWebServer.RegisterPage("/xnl/battery",  "Battery config",   WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xvv/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xvv/pwrmon", "Power Monitor", WebPowerMon, PageMenu_Vehicle, PageAuth_Cookie);

}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVectrixVX1::WebDeInit()
{
  //MyWebServer.DeregisterPage("/xnl/features");
  //MyWebServer.DeregisterPage("/xnl/battery");
  MyWebServer.DeregisterPage("/xvv/cellmon");
  MyWebServer.DeregisterPage("/xvv/pwrmon");

}

/**
 * GetDashboardConfig: Vectrix VX1 specific dashboard setup
 */
void OvmsVehicleVectrixVX1::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 50, className: 'green-band' },"
        "{ from: 50, to: 80, className: 'yellow-band' },"
        "{ from: 80, to: 120, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 100, max: 150,"
      "plotBands: ["
        "{ from: 100, to: 110, className: 'red-band' },"
        "{ from: 110, to: 120, className: 'yellow-band' },"
        "{ from: 120, to: 140, className: 'green-band' },"
        "{ from: 140, to: 150, className: 'red-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency: Wh/km
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 45, className: 'green-band' },"
        "{ from: 45, to: 60, className: 'yellow-band' },"
        "{ from: 60, to: 120, className: 'red-band' }]"
    "},{"
      // Power: Kw
      "min: -3, max: 15,"
      "plotBands: ["
        "{ from: -3, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 7, className: 'green-band' },"
        "{ from: 7, to: 10, className: 'yellow-band' },"
        "{ from: 10, to: 15, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 5, max: 85, tickInterval: 20,"
      "plotBands: ["
        "{ from: 5, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 85, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 15,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 45, className: 'normal-band border' },"
        "{ from: 45, to: 60, className: 'red-band border' }]"
    "},{"
      // Motor Controller temperature:
      "min: 5, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 5, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 100, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 90, className: 'normal-band border' },"
        "{ from: 90, to: 100, className: 'red-band border' }]"
    "}]";
}

/**
 * WebPowerMon: (/xvv/pwrmon)
 */
void OvmsVehicleVectrixVX1::WebPowerMon(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<style type=\"text/css\">\n"
    ".metric.number .value { min-width: 5em; }\n"
    ".metric.number .unit { min-width: 3em; }\n"
    ".night .highcharts-color-1 {\n"
      "fill: #c3c3c8;\n"
      "stroke: #c3c3c8;\n"
    "}\n"
    ".night .highcharts-legend-item text {\n"
      "fill: #dddddd;\n"
    "}\n"
    ".highcharts-graph {\n"
      "stroke-width: 4px;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "fill: #000000;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "color: #bbb;\n"
    "}\n"
    "#dynochart {\n"
      "width: 100%;\n"
      "max-width: 100%;\n"
      "height: 50vh;\n"
      "min-height: 265px;\n"
    "}\n"
    ".fullscreened #dynochart {\n"
      "height: 80vh;\n"
    "}\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary panel-single\">\n"
      "<div class=\"panel-heading\">Power Monitor</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"row receiver\" id=\"livestatus\">\n"
          "<div class=\"table-responsive\">\n"
            "<table class=\"table table-bordered table-condensed\">\n"
              "<tbody>\n"
                "<tr>\n"
                  "<th>Speed</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.p.speed\"><span class=\"value\">?</span><span class=\"unit\">Km/h</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.throttle\"><span class=\"value\">?</span><span class=\"unit\">%thr</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.footbrake\"><span class=\"value\">?</span><span class=\"unit\">%brk</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.p.gpsspeed\"><span class=\"value\">?</span><span class=\"unit\">GPSKm/h</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Battery</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.voltage\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.current\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.power\"><span class=\"value\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.cac\"><span class=\"value\">?</span><span class=\"unit\">Ah CAC</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.energy.used\"><span class=\"value\">?</span><span class=\"unit\">kWh Used</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.energy.recd\"><span class=\"value\">?</span><span class=\"unit\">KWh Recv</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Charger</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.c.voltage\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.c.current\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.c.time\"><span class=\"value\">?</span><span class=\"unit\">Sec</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.c.temp\"><span class=\"value\">?</span><span class=\"unit\">°C Temp</span></div>\n"
                    "<div class=\"metric text\" data-prec=\"1\" data-metric=\"v.c.state\"><span class=\"label\">Last State:</span><span class=\"value\">?</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"0\" data-metric=\"v.c.charging\"><span class=\"value\">?</span><span class=\"unit\">State</span></div>\n"
                  "</td>\n"
                "</tr>\n"
              "</tbody>\n"
            "</table>\n"
          "</div>\n"
          "<div id=\"dynochart\"></div>\n"
        "</div>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
    "</div>\n"
    "\n"
    "\n"
    "<script>\n"
    "\n"
    "/**\n"
     "* Form handling\n"
     "*/\n"
    "\n"
    "/**\n"
     "* Dyno chart\n"
     "*/\n"
    "\n"
    "var dynochart;\n"
    "\n"
    "function get_dyno_data() {\n"
      "var data = {\n"
        "bat_pwr_drv: metrics[\"v.b.energy.used\"],\n"
        "bat_pwr_rec: metrics[\"v.b.energy.recd\"],\n"
      "};\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart.series[0].setData(data.bat_pwr_drv);\n"
      "dynochart.series[1].setData(data.bat_pwr_rec);\n"
    "}\n"
    "\n"
    "function init_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart = Highcharts.chart('dynochart', {\n"
        "chart: {\n"
          "type: 'spline',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"v.b.energy.used\"] != null\n"
                 "|| update[\"v.b.energy.recd\"] != null)\n"
                  "update_dyno_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'x',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
        "},\n"
        "xAxis: {\n"
          "minTickInterval: 5,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "gridLineWidth: 1,\n"
        "},\n"
        "plotOptions: {\n"
          "series: {\n"
            "pointStart: 1,\n"
          "},\n"
          "spline: {\n"
            "marker: {\n"
              "enabled: false,\n"
            "},\n"
          "},\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "crosshairs: true,\n"
          "useHTML: true,\n"
          "headerFormat: '<table>' +\n"
            "'<tr><td class=\"tt-header\">Speed:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.key} kph</b></td></tr>',\n"
          "pointFormat:\n"
            "'<tr><td class=\"tt-color-{point.colorIndex}\">{series.name}:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.y}</b></td></tr>',\n"
          "footerFormat: '</table>',\n"
          "valueDecimals: 1,\n"
        "},\n"
        "yAxis: [{\n"
          "title: { text: \"Power [kW]\" },\n"
          "labels: { format: \"{value:.0f}\" },\n"
          "min: 0,\n"
          "minTickInterval: 1,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "showFirstLabel: false,\n"
        "}],\n"
        "responsive: {\n"
          "rules: [{\n"
            "condition: { maxWidth: 500 },\n"
            "chartOptions: {\n"
              "yAxis: [{\n"
                "title: { text: null },\n"
              "},{\n"
                "title: { text: null },\n"
              "}],\n"
            "},\n"
          "}],\n"
        "},\n"
        "series: [{\n"
          "name: 'Max Drive Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_drv,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Recup Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_rec,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#dynochart').data('chart', dynochart).addClass('has-chart');"
    "}\n"
    "\n"
    "/**\n"
     "* Chart initialization\n"
     "*/\n"
    "\n"
    "function init_charts() {\n"
      "init_dyno_chart();\n"
    "}\n"
    "\n"
    "if (window.Highcharts) {\n"
      "init_charts();\n"
    "} else {\n"
      "$.ajax({\n"
        "url: \"" URL_ASSETS_CHARTS_JS "\",\n"
        "dataType: \"script\",\n"
        "cache: true,\n"
        "success: function(){ init_charts(); }\n"
      "});\n"
    "}\n"
    "\n"
    "</script>\n"
  );

  PAGE_HOOK("body.post");
  c.done();
}


#endif //CONFIG_OVMS_COMP_WEBSERVER
