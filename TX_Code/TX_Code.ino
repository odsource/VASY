void setup() {
  // put your setup code here, to run once:
  // initialize serial communication:
  Serial.begin(9600);
  Serial1.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial1.write('H');
  Serial.write('H');
  delay(2000);
  Serial.write('L');
  Serial1.write('L');  
  delay(2000);
}
