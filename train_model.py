import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
import joblib

# Load dataset
df = pd.read_csv("./fire_sensor_dataset_1000.csv")

# Encode target variable (alert_intensity)
df["alert_intensity"] = df["alert_intensity"].map({"Low": 0, "Medium": 1, "High": 2})

# Select features and target
X = df[["flame", "smoke", "temperature", "humidity"]]
y = df["alert_intensity"]

# Split dataset
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Train model
model = RandomForestClassifier(n_estimators=100, random_state=42)
model.fit(X_train, y_train)

# Save model
joblib.dump(model, "fire_model.pkl")

# Print accuracy
print("Model trained successfully!")
print("Accuracy:", model.score(X_test, y_test))