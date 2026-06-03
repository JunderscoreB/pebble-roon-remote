FROM node:18-alpine

WORKDIR /app

# Copy package configurations first to leverage Docker layer caching
COPY package*.json ./

# Install git so npm can clone the required GitHub dependencies
RUN apk add --no-cache git

# Install dependencies
RUN npm install

# Copy the rest of the application code
COPY . .

EXPOSE 3000

# Start the bridge
CMD ["node", "index.js"]
