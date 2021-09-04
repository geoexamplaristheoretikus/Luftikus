// function to compute absolute temp
float compabshum (float temperature, float percentHumidity)
{
  // Calculate the absolute humidity in g/m³
  // https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/

  float absHumidity;
  float absTemperature;
  absTemperature = temperature + 273.15;

  absHumidity = 6.112;
  absHumidity *= exp((17.67 * temperature) / (243.5 + temperature));
  absHumidity *= percentHumidity;
  absHumidity *= 2.1674;
  absHumidity /= absTemperature;

  return absHumidity;
}

// Calculare dew point in Celcius.
float compdewpoint (float temperature, float percentHumidity)
{
  // Calculate the dewpoint
  // https://carnotcycle.wordpress.com/2017/08/01/compute-dewpoint-temperature-from-rh-t/

  float dewpoint;
  dewpoint = 243.5;
  dewpoint *= (log(percentHumidity/100) + ((17.67 * temperature) /(243.5 + temperature)));
  dewpoint /= (17.67- log(percentHumidity/100) - ((17.67 * temperature) /(243.5 + temperature)));

  return dewpoint;
}

float humidity_err (float Humidity)
{
  // Calculate the error of dht22 humidity reading
  float err;
  if (Humidity <= 80.0 && Humidity >= 20.0)
  {err = 2;}
  else if (Humidity >= 90.0  || Humidity  <= 10.0)
  {err = 5;}
  else if (Humidity > 80.0)
  {err = 2 + (Humidity - 80.0)*0.3;}
  else {err = 5 - (Humidity - 10.0)*0.3;}
  return err;
}
